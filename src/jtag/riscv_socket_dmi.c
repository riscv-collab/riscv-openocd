#include "riscv_socket_dmi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "drivers/riscv_dtm/dtm.h"
#include "transport/transport.h"
#include "target/target.h"
#include "target/riscv/riscv.h"
#include "helper/log.h"

#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

typedef struct {
    int sockfd;
    char host[256];
    int port;
} socket_priv_t;

// Commands for serialized socket message
#define READ_COMMAND 0x00
#define WRITE_COMMAND 0x01

// Response codes
#define RESPONSE_OK 0x00
#define RESPONSE_ERROR 0x01

#define DEFAULT_SOCKET_HOST "127.0.0.1"
#define DEFAULT_SOCKET_PORT 5555

static int transportIsRegistered = -1;

int clear_socket_buffer(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return -1;
    }

    // Make socket non-blocking
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL, O_NONBLOCK)");
        return -1;
    }

    fd_set readfds;
    struct timeval timeout;
    int ret;
    char buf[1024]; // Buffer to discard data

    do {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Set a short timeout (e.g., 100 milliseconds)
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret == -1) {
            perror("select");
            return -1;
        } else if (ret > 0) {
            // Data is available to read
            ssize_t bytes_read = recv(sockfd, buf, sizeof(buf), 0);
            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more data (expected in non-blocking mode)
                    break;
                } else {
                    perror("recv");
                    return -1;
                }
            }
        }
    } while (ret > 0);

    // Restore original socket flags (if needed)
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL, flags)");
        return -1;
    }

    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len) {
    size_t to_read = len;
    char *temp_buf = (char *)buf;
    size_t total_bytes_read = 0;

    while (to_read > 0) {
        ssize_t bytes_read = recv(sockfd, temp_buf + total_bytes_read, to_read, 0);

        if (bytes_read < 0) {
            LOG_ERROR("recv error: %s", strerror(errno));
            return ERROR_FAIL; 
        } else if (bytes_read == 0) {
            LOG_ERROR("Socket closed by remote host");
            return ERROR_FAIL;
        }

        total_bytes_read += bytes_read;
        to_read -= bytes_read;
    }

    return total_bytes_read;
}

static int socket_dmi_connect(dtm_driver_t* driver){
    if (!driver->priv) {
        return ERROR_FAIL;
    }

    socket_priv_t *priv = (socket_priv_t *)driver->priv;
    if (priv->sockfd != -1) {
        LOG_WARNING("Already connected, close it first.");
        close(priv->sockfd);
        priv->sockfd = -1;
    }

    priv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (priv->sockfd < 0) {
        perror("Socket creation failed");
        return ERROR_FAIL;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(priv->port);

    if (inet_pton(AF_INET, priv->host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(priv->sockfd);
        priv->sockfd = -1;
        return ERROR_FAIL;
    }

    if (connect(priv->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        close(priv->sockfd);
        priv->sockfd = -1;
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

int socket_dmi_init(dtm_driver_t *driver) {
    // Use default values for now and later see if there is something in the openocd config file to connect with
    // like socket specified host and port from the config file
    socket_priv_t *priv = malloc(sizeof(socket_priv_t));
    if (!priv) {
        LOG_ERROR("Failed to allocate memory for socket_priv_t.");
        return ERROR_FAIL;
    }

    strncpy(priv->host, DEFAULT_SOCKET_HOST, sizeof(priv->host) - 1);
    priv->host[sizeof(priv->host) - 1] = '\0';
    priv->port = DEFAULT_SOCKET_PORT;

    priv->sockfd = -1; // Initialize to -1 to indicate not connected yet
    driver->priv = priv;
    return ERROR_OK;
}


int socket_dmi_deinit(dtm_driver_t *driver) {
    if (!driver->priv) {
        return ERROR_FAIL;
    }

    socket_priv_t *priv = (socket_priv_t *)driver->priv;
    if (priv->sockfd != -1) {
        close(priv->sockfd);
    }
    free(priv);
    driver->priv = NULL;
    return ERROR_OK;
}

int socket_dmi_read_dmi(dtm_driver_t *driver, uint32_t *data, uint32_t address) {
    if (data == NULL) {
        LOG_ERROR("data passed to socket_dmi_read_dmi function is nullptr! Make sure to examine which pointer is passed to socket_dmi_read_dmi");
        return ERROR_FAIL;
    }

    if (driver == NULL) {
        LOG_ERROR("driver passed to socket_dmi_read_dmi function is nullptr! Make sure to examine which pointer is passed to socket_dmi_read_dmi");
        return ERROR_FAIL;
    }

    LOG_INFO("socket_dmi_read_dmi from address: address: %x", address);
    if (!driver->priv) {
        return ERROR_FAIL;
    }

    socket_priv_t *priv = (socket_priv_t *)driver->priv;

    // Check if connected and connect if necessary
    if (priv->sockfd == -1) {
        if (socket_dmi_connect(driver) != 0) {
            LOG_ERROR("Not connected and unable to establish connection.");
            return -1;
        }
    }
    const size_t data_length = 0;
    const ssize_t request_size = 6; // Cmd(1) + Addr(4) + Len(1)
    uint8_t buffer[request_size];
    // Protocol: [READ_COMMAND][address (4 bytes)][data_length (1 byte, fixed as 4 for now)][Reserved (3 bytes)]
    buffer[0] = READ_COMMAND; 
    buffer[1] = (address >> 24) & 0xFF;
    buffer[2] = (address >> 16) & 0xFF;
    buffer[3] = (address >> 8) & 0xFF;
    buffer[4] = address & 0xFF;
    buffer[5] = data_length; // data length in bytes (fixed at 4)

    // if (clear_socket_buffer(priv->sockfd) != 0) {
    //     LOG_ERROR("Failed to clear socket buffer in socket_dmi_read_dmi");
    //     return ERROR_FAIL;
    // }

    ssize_t sent = send(priv->sockfd, buffer, request_size, 0);
    if (sent != request_size) { // Check against request_size
        perror("Failed to send read DMI command");
        return ERROR_FAIL;
    }

    const size_t response_length = 1;
    uint8_t response_buffer[response_length];
    size_t received_bytes = recv_all(priv->sockfd, response_buffer, response_length);
    if (received_bytes != response_length) {
        LOG_ERROR("Failed to receive response byte: received %zd bytes, expected 1", received_bytes);
        return ERROR_FAIL;
    }

    if (response_buffer[0] != RESPONSE_OK) {
        LOG_ERROR("DMI read failed (error code: 0x%X)", response_buffer[0]);
        return -1;
    }

    const size_t response_data_length = 4; // Expect 4 bytes of data in response
    uint8_t data_buffer[response_data_length];
    received_bytes = recv_all(priv->sockfd, data_buffer, response_data_length);

    if (received_bytes != response_data_length) {
        LOG_ERROR("Failed to receive data: received %zd bytes, expected %zd", received_bytes, response_data_length);
        return ERROR_FAIL;
    }

    *data = (data_buffer[0] << 24) |
            (data_buffer[1] << 16) |
            (data_buffer[2] << 8) |
            data_buffer[3];

    LOG_INFO("Received DMI read: addr=0x%08x, data=0x%08x", address, *data);

    return ERROR_OK;
}

int socket_dmi_write_dmi(dtm_driver_t *driver, uint32_t address, uint32_t data) {
    LOG_INFO("socket_dmi_write_dmi: address: %x, data: %x", address, data);
    if (!driver->priv) {
        return ERROR_FAIL;
    }

    socket_priv_t *priv = (socket_priv_t *)driver->priv;

    // Check if connected and connect if necessary
    if (priv->sockfd == -1) {
        if (socket_dmi_connect(driver) != 0) {
            LOG_ERROR("Not connected and unable to establish connection.");
            return -1;
        }
    }
    uint8_t buffer[10];
    // Protocol: [WRITE_COMMAND][address (4 bytes)][data_length (1 byte, fixed as 4 for now)][data (4 bytes)]
    buffer[0] = WRITE_COMMAND;
    buffer[1] = (address >> 24) & 0xFF;
    buffer[2] = (address >> 16) & 0xFF;
    buffer[3] = (address >> 8) & 0xFF;
    buffer[4] = address & 0xFF;
    buffer[5] = 4; // data length in bytes (fixed at 4)
    buffer[6] = (data >> 24) & 0xFF;
    buffer[7] = (data >> 16) & 0xFF;
    buffer[8] = (data >> 8) & 0xFF;
    buffer[9] = data & 0xFF;

    // if (clear_socket_buffer(priv->sockfd) != 0) {
    //     LOG_ERROR("Failed to clear socket buffer in socket_dmi_write_dmi");
    //     return ERROR_FAIL;
    // }

    ssize_t sent = send(priv->sockfd, buffer, sizeof(buffer), 0);
    if (sent != sizeof(buffer)) {
        perror("Failed to send write DMI command");
        return ERROR_FAIL;
    }

    uint8_t recv_buffer[1];
    ssize_t received_bytes = recv_all(priv->sockfd, recv_buffer, 1);
    if (received_bytes != 1) {
        return ERROR_FAIL;
    }

    if (recv_buffer[0] != RESPONSE_OK) {
        LOG_ERROR("DMI write failed (error code: 0x%X)", buffer[0]);
        return -1;
    }

    return ERROR_OK;
}

COMMAND_HANDLER(command_socket_host){
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;
    dtm_driver_t *active_driver = get_active_dtm_driver();
    if (!active_driver || strcmp(active_driver->name, "socket") != 0) {
        LOG_ERROR("Active DTM driver is not socket");
        return ERROR_FAIL;
    }
    socket_priv_t *priv = (socket_priv_t*)active_driver->priv;
    strncpy(priv->host, CMD_ARGV[0], sizeof(priv->host) - 1);
    priv->host[sizeof(priv->host) - 1] = '\0';
    return ERROR_OK;
}

COMMAND_HANDLER(command_socket_port){
	if (CMD_ARGC != 1)
		return ERROR_COMMAND_SYNTAX_ERROR;
    dtm_driver_t *active_driver = get_active_dtm_driver();
    if (!active_driver || strcmp(active_driver->name, "socket") != 0) {
        LOG_ERROR("Active DTM driver is not socket'");
        return ERROR_FAIL;
    }

    socket_priv_t *priv = (socket_priv_t*)active_driver->priv;
    priv->port = atoi(CMD_ARGV[0]);
    return ERROR_OK;
}

COMMAND_HANDLER(command_socket_connect){
    dtm_driver_t *active_driver = get_active_dtm_driver();
    if (!active_driver || strcmp(active_driver->name, "socket") != 0) {
        LOG_ERROR("Active DTM driver is not socket");
        return ERROR_FAIL;
    }

    if (socket_dmi_connect(active_driver) != 0) {
        LOG_ERROR("Failed to connect to socket.");
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

static int socket_dtm_set_transport(struct transport *transport)
{
    // This function is called to set the transport
    // In this case, we should verify the transport is "socket"
    if (strcmp(transport->name, "socket") != 0) {
        LOG_ERROR("transport->name != socket in socket_dtm_set_transport");
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

static dtm_driver_t socket_driver __attribute__((used)) = {
    .name = "socket",
    .init = socket_dmi_init,
    .deinit = socket_dmi_deinit,
    .read_dmi = socket_dmi_read_dmi,
    .write_dmi = socket_dmi_write_dmi,
    .read_dtmcontrol = socket_dtmcontrol_read_dmi,
    .set_transport = socket_dtm_set_transport,
    .priv = NULL
};


// Used for simulating DTMCONTROL, for socket simulation we do not need to simulate DTM directly. 
int socket_dtmcontrol_read_dmi(struct dtm_driver *driver, uint32_t *value)
{
    LOG_INFO("Faking DTMCONTROL read in socket transport");
    *value = (1 << 0) | (6 << 4);  // version=1, abits=6
    return ERROR_OK;
}

int socket_dmi_read(struct target *target, uint32_t *data, uint32_t address) {
    return socket_dmi_read_dmi(&socket_driver, data, address);
}

int socket_dmi_write(struct target *target, uint32_t address, uint32_t data) {
    return socket_dmi_write_dmi(&socket_driver, address, data);
}


int socket_dtmcontrol_read(struct target *target, uint32_t *value)
{
    return socket_dtmcontrol_read_dmi(&socket_driver, value);
}


static const struct command_registration socket_transport_command_handlers[] = {
    {
        .name = "host",
        .mode = COMMAND_ANY,
        .help = "Set the hostname for the socket connection",
        .usage = "<hostname>",
        .handler = command_socket_host,
    },
    {
        .name = "port",
        .mode = COMMAND_ANY,
        .help = "Set the port for the socket connection",
        .usage = "<port>",
        .handler = command_socket_port,
    },
    {
        .name = "connect",
        .mode = COMMAND_ANY,
        .help = "Connect to the socket server",
        .usage = "",
        .handler = command_socket_connect,
    },
    COMMAND_REGISTRATION_DONE
};

static int socket_transport_select(struct command_context *ctx) {
    struct target *target = get_current_target(ctx);
    if (!target){
        LOG_ERROR("Target not yet selected, socket transport will wait.");
        return ERROR_FAIL;
    }

    // if (strcmp(target->type->name, "riscv") != 0) {
    //     LOG_DEBUG("Target is not a riscv target, skipping socket transport.");
    //     return ERROR_OK; // or ERROR_FAIL depending on your policy
    // }

    LOG_INFO("Selected socket transport for RISC-V target.");
    RISCV_INFO(r);
    r->dmi_read = socket_dmi_read;
    r->dmi_write = socket_dmi_write;
    return register_commands(ctx, NULL, socket_transport_command_handlers);
}

static int socket_transport_init(struct command_context *cmd_ctx)
{
    if (socket_dmi_init(&socket_driver) != 0) {
        LOG_ERROR("Failed to register socket DMI driver.");
        return ERROR_FAIL;
    } else {
        LOG_INFO("Socket DMI driver registered successfully.");
    }

    //return socket_dmi_init(&socket_driver);
    return ERROR_OK;
}

static struct transport socket_transport __attribute__((used)) = {
    .name = "socket",
    .select = socket_transport_select,
    .init = socket_transport_init,
    .next = NULL,
};

int socket_transport_register(void) {
    return transport_register(&socket_transport);
}

int register_socket_dtm_driver(void) {
    return register_dtm_driver(&socket_driver);
}

int register_riscv_socket_transport(void) {
    LOG_INFO("Initializing socket DTM driver.");
    if (register_socket_dtm_driver() != 0) {
        LOG_ERROR("Failed to register socket DTM driver.");
        return ERROR_FAIL;
    } else {
        LOG_INFO("Socket DTM driver registered successfully.");
    }

    if (transportIsRegistered != -1){
        LOG_INFO("Socket transport already registered successfully.");
        return ERROR_OK;
    }
    LOG_INFO("Initializing socket transport.");
    if (socket_transport_register() != ERROR_OK) {
        LOG_ERROR("Failed to register socket transport.");
        return ERROR_FAIL;
    } else {
        transportIsRegistered = 1;
        LOG_INFO("Socket transport registered successfully.");
    }

    return ERROR_OK;

}

static void socket_constructor(void) __attribute__ ((constructor));
static void socket_constructor(void)
{
    if(register_riscv_socket_transport() != 0){
        LOG_ERROR("Failed to initialize socket transport!");
    }
    LOG_INFO("Socket transport successfully initialized.");
}

bool transport_is_socket(void)
{
	return get_current_transport() == &socket_transport;
}