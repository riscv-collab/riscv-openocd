import socket
import struct
import time
# import ipdb
import binascii
import threading

HOST = 'localhost'
PORT = 5555

READ_COMMAND = 0x00
WRITE_COMMAND = 0x01

RESPONSE_OK = 0x00
RESPONSE_ERROR = 0x01

XLEN = 64
misa = 0x800000000014112d  # RV64IMAFDCV (Example, includes 'V')
NUM_GPRS = 32
gprs = [0] * NUM_GPRS
dcsr = 0x40000003  # Default initial state
dpc = 0
mstatus = 0xA00000200

# vlenb: Vector Length in Bytes. Only valid if 'V' extension is present.
# VLEN=128 -> vlenb=16. VLEN=512 -> vlenb=64. Let's assume VLEN=128.
vlenb = 16 if (misa >> 21) & 1 else 0 # Check 'V' bit (bit 21) in misa

mtopi = 0

DMI_DATA0 = 0x04
DMI_DATA1 = 0x05
DMI_DMCONTROL = 0x10
DMI_DMSTATUS = 0x11
DMI_HARTINFO = 0x12
DMI_ABSTRACTCS = 0x16
DMI_COMMAND = 0x17
DMI_ABSTRACTAUTO = 0x18
DMI_PROGBUF0 = 0x20
DMI_SBCS = 0x38

CSR_MISA = 0x301
CSR_MSTATUS = 0x300
CSR_DCSR = 0x7B0
CSR_DPC = 0x7B1
CSR_VLENB = 0xC22
CSR_MTOPI = 0x350
GPR_BASE = 0x1000

dmi_mem = {
    DMI_DMCONTROL: 0x0,     # Start with hart halted, dmactive=0
    DMI_DMSTATUS: (1 << 8) | (1 << 9) | (1 << 7) | 0x2, # 0x00000382 : set both allhalted and anyhlated
    DMI_HARTINFO: 0x00102004, # nscratch=1(0+1), dataaccess=0(csr), datasize=2(DATA0/1), dataaddr=0x04
    DMI_ABSTRACTCS: (0 << 8) | (0 << 12) | (2 << 0) | (2 << 24), # Should be 0x02000002 initially
    DMI_COMMAND: 0x00000000,
    DMI_ABSTRACTAUTO: 0x00000000,
    DMI_DATA0: 0x00000000,
    DMI_DATA1: 0x00000000,
    DMI_PROGBUF0: 0x00000000,
    DMI_SBCS: 0x00000000,
}

def update_cmderr(error_code):
    # Preserve datacount and progbufsize when updating cmderr and busy
    preserved_bits = dmi_mem[DMI_ABSTRACTCS] & (0xFFF | (0x1F << 24)) # Mask for datacount and progbufsize
    dmi_mem[DMI_ABSTRACTCS] = preserved_bits | ((error_code & 0x7) << 8) # Set cmderr
    # Clear busy bit (bit 12)
    dmi_mem[DMI_ABSTRACTCS] &= ~(1 << 12)

# --- Helper: Check if V extension is enabled in simulated misa ---
def has_v_extension():
    return (misa >> 21) & 1 # Check 'V' bit (bit 21)

# --- Helper: Check if Smtopi extension is enabled (placeholder) ---
def has_smtopi_extension():
    return False # Placeholder

def handle_dmi_read(address, conn):
    """Handles DMI read requests."""
    global dmi_mem
    data = dmi_mem.get(address, 0) # Default to 0 if address not found

    print(f"DMI Read : Addr=0x{address:02X}, Data=0x{data:08X}")

    # Specific handling if needed (e.g., dynamic status)
    if address == DMI_DMSTATUS:
        # For now, just returning the stored value.
        # Example: Ensure version is correct
        data = (data & ~0xF) | 0x2 # Force version 0.13
        print(f"  Adjusted DMI_DMSTATUS read: 0x{data:08X}")
        dmi_mem[address] = data # Update stored value if adjusted

    elif address == DMI_ABSTRACTCS:
        print(f"  DMI_ABSTRACTCS read: 0x{data:08X}")

    elif address == DMI_DATA0 or address == DMI_DATA1:
        print(f"  DMI_DATA{address-DMI_DATA0} read: 0x{data:08X}")

    response = struct.pack(">B", RESPONSE_OK) + struct.pack(">I", data)
    conn.sendall(response)
    return data


def handle_dmi_write(address, data, conn):
    """Handles DMI write requests."""
    global dmi_mem
    print(f"DMI Write: Addr=0x{address:04X}, Data=0x{data:08X}") # Use 04X for address

    # --- Handle specific DMI register writes ---
    if address == DMI_DMCONTROL:
        requested_dmcontrol = data
        print(f"  DMI_DMCONTROL write request: 0x{requested_dmcontrol:08X}")

        current_dmcontrol = dmi_mem.get(DMI_DMCONTROL, 0)
        was_active = (current_dmcontrol & 1) != 0

        # Simulate state changes based on request
        haltreq = (requested_dmcontrol >> 31) & 1
        resumereq = (requested_dmcontrol >> 30) & 1
        hartreset = (requested_dmcontrol >> 29) & 1
        ackhavereset = (requested_dmcontrol >> 28) & 1
        ndmreset_req = (requested_dmcontrol >> 1) & 1 # Check if DM reset is requested
        dmactive_req = (requested_dmcontrol >> 0) & 1 # Check if activation is explicitly requested

        new_dmcontrol_state = requested_dmcontrol

        if ndmreset_req:
            # If ndmreset is requested, DM MUST become inactive.
            new_dmcontrol_state &= ~1 # Force dmactive (bit 0) to 0
            print("  -> ndmreset requested, forcing dmactive=0")
        elif not was_active and dmactive_req == 0:
            # If the DM wasn't already active, and the first write requests
            # dmactive=0 (like the initial examine write), force it active.
            # This simulates the DM activating on first access.
            new_dmcontrol_state |= 1 # Force dmactive (bit 0) to 1
            print("  -> First access with dmactive=0 requested, forcing dmactive=1")
        # else:
            # Otherwise (DM was already active, or dmactive=1 was requested),
            # respect the dmactive bit written by the debugger.
            # Since new_dmcontrol_state started as requested_dmcontrol,
            # dmactive is already correctly set (or cleared).
            pass
        # Store the potentially modified state
        dmi_mem[DMI_DMCONTROL] = new_dmcontrol_state
        print(f"  Simulated DMI_DMCONTROL state stored: 0x{dmi_mem[DMI_DMCONTROL]:08X}")


        # --- Update DMSTATUS based on the *requested* control bits ---
        status = dmi_mem.get(DMI_DMSTATUS, 0) # Get current status
        # Clear bits that might change based on requests
        status &= ~((1 << 9) | (1 << 11) | (1 << 17) | (1 << 19)) # Clear allhalted, allrunning, allresumeack, allhavereset

        if haltreq:
            print("  -> Halt Request")
            status |= (1 << 9) # Set allhalted (assuming only one hart)
            # status &= ~(1 << 11) # Clear allrunning (already cleared above)
            # status &= ~(1 << 17) # Clear allresumeack (already cleared above)
        elif resumereq:
            print("  -> Resume Request")
            # status &= ~(1 << 9) # Clear allhalted (already cleared above)
            status |= (1 << 11) # Set allrunning
            status |= (1 << 17) # Set allresumeack
        else:
            # No explicit halt or resume request in this write.
            # Assume HALTED state, especially after reset or initial examine.
            print("  -> No halt/resume req, ensuring HALTED state")
            status |= (1 << 9) # Default to halted

        if hartreset:
            print("  -> Hart Reset Request")
            status |= (1 << 19) # Set allhavereset
        if ackhavereset:
            print("  -> Ack Havereset")
            pass
            # status &= ~(1 << 19) # Clear allhavereset (already cleared above)


        status |= (1 << 7) # authenticated
        status = (status & ~0xF) | 0x2 # version=0.13

        if (status >> 9) & 1: # If allhalted (bit 9) is set
            status |= (1 << 8) # Set anyhalted (bit 8)

        dmi_mem[DMI_DMSTATUS] = status
        print(f"  Updated DMI_DMSTATUS: 0x{status:08X}")

    elif address == DMI_COMMAND:
        print(f"  -> Abstract Command Written: 0x{data:08X}")
        dmi_mem[address] = data # Store the command
        # Set busy bit in ABSTRACTCS immediately
        dmi_mem[DMI_ABSTRACTCS] |= (1 << 12)
        print(f"  Set Busy. ABSTRACTCS: 0x{dmi_mem[DMI_ABSTRACTCS]:08X}")
        # Execute (this function should update cmderr and clear busy bit upon completion)
        execute_abstract_command(data)
        print(f"  <- Abstract Command Complete. ABSTRACTCS: 0x{dmi_mem[DMI_ABSTRACTCS]:08X}")

    elif address == DMI_DATA0 or address == DMI_DATA1:
        print(f"  -> DMI_DATA{address-DMI_DATA0} write: 0x{data:08X}")
        dmi_mem[address] = data

    else:
        dmi_mem[address] = data

    conn.sendall(struct.pack(">B", RESPONSE_OK))

def execute_abstract_command(command):
    """Executes abstract commands (simplified)."""
    global dcsr, dpc, misa, vlenb, mtopi, mstatus, gprs, dmi_mem

    command_type = (command >> 24) & 0xFF

    if command_type == 0:
        # Extract parameters from the command word
        reg_num = command & 0xFFFF
        write = (command >> 16) & 1 # Correct bit position for Access Reg cmd
        transfer = (command >> 17) & 1 # Correct bit position
        postexec = (command >> 18) & 1 # Not handled here
        aarpostincrement = (command >> 19) & 1 # Not handled here
        aarsize = (command >> 20) & 0x7 # 2=32bit, 3=64bit

        print(f"  Abstract CMD: AccessReg: reg=0x{reg_num:04X}, write={write}, transfer={transfer}, size={aarsize}")

        # Check if size matches XLEN for CSRs/GPRs (basic check)
        expected_aarsize = 3 if XLEN == 64 else 2
        # DCSR is always 32-bit access regardless of XLEN
        is_dcsr_access = (reg_num == CSR_DCSR)

        if transfer: # Perform the register access
            cmderr = 0 # Assume success initially
            data_val_low = dmi_mem.get(DMI_DATA0, 0)
            data_val_high = dmi_mem.get(DMI_DATA1, 0)
            data_val_64 = (data_val_high << 32) | data_val_low

            if write:
                if reg_num >= GPR_BASE and reg_num < (GPR_BASE + NUM_GPRS):
                    gpr_index = reg_num - GPR_BASE
                    if aarsize == expected_aarsize:
                        gprs[gpr_index] = data_val_64 if XLEN == 64 else data_val_low
                        print(f"    WRITE GPR{gpr_index} = 0x{gprs[gpr_index]:X}")
                    else: cmderr = 2 # Size mismatch
                elif reg_num == CSR_DCSR:
                    if aarsize == 2: # DCSR is 32-bit access
                        dcsr = data_val_low
                        print(f"    WRITE DCSR = 0x{dcsr:08X}")
                    else: cmderr = 2 # Size mismatch
                elif reg_num == CSR_DPC:
                     if aarsize == expected_aarsize:
                        dpc = data_val_64 if XLEN == 64 else data_val_low
                        print(f"    WRITE DPC = 0x{dpc:X}")
                     else: cmderr = 2 # Size mismatch
                # elif reg_num == CSR_MSTATUS: ...
                else:
                    print(f"    WRITE to unsupported/read-only register 0x{reg_num:04X}")
                    cmderr = 4 # Not supported

            else:
                read_val_low = 0
                read_val_high = 0
                read_val_64 = 0

                if reg_num >= GPR_BASE and reg_num < (GPR_BASE + NUM_GPRS):
                    gpr_index = reg_num - GPR_BASE
                    if aarsize == expected_aarsize:
                        read_val_64 = gprs[gpr_index]
                        print(f"    READ GPR{gpr_index} = 0x{read_val_64:X}")
                    else: cmderr = 2 # Size mismatch
                elif reg_num == CSR_MISA:
                    if aarsize == expected_aarsize:
                        read_val_64 = misa
                        print(f"    READ MISA = 0x{read_val_64:016X}")
                    else: cmderr = 2
                elif reg_num == CSR_MSTATUS:
                     if aarsize == expected_aarsize:
                        read_val_64 = mstatus
                        print(f"    READ MSTATUS = 0x{read_val_64:016X}")
                     else: cmderr = 2
                elif reg_num == CSR_DCSR:
                    if aarsize == 2: # DCSR is 32-bit access
                        read_val_64 = dcsr & 0xFFFFFFFF # Ensure 32-bit value
                        print(f"    READ DCSR = 0x{read_val_64:08X}")
                    else: cmderr = 2
                elif reg_num == CSR_DPC:
                    if aarsize == expected_aarsize:
                        read_val_64 = dpc
                        print(f"    READ DPC = 0x{read_val_64:X}")
                    else: cmderr = 2
                elif reg_num == CSR_VLENB:
                    if has_v_extension():
                        if aarsize == expected_aarsize: # vlenb size matches XLEN
                            read_val_64 = vlenb
                            print(f"    READ VLENB = 0x{read_val_64:X}")
                        else: cmderr = 2
                    else:
                        print(f"    READ VLENB failed: V extension not present")
                        cmderr = 4 # Not supported (or 1=busy if check takes time)
                elif reg_num == CSR_MTOPI:
                    # Check if Smtopi is notionally present if needed
                    # if has_smtopi_extension():
                    if aarsize == expected_aarsize: # mtopi size matches XLEN
                        read_val_64 = mtopi
                        print(f"    READ MTOPI = 0x{read_val_64:X}")
                    else: cmderr = 2
                    # else:
                    #    print(f"    READ MTOPI failed: Smtopi extension not present")
                    #    cmderr = 4 # Not supported
                else:
                    print(f"    READ from unsupported register 0x{reg_num:04X}")
                    cmderr = 4 # Not supported

                # Store the read value back into DMI DATA registers
                if cmderr == 0:
                    dmi_mem[DMI_DATA0] = read_val_64 & 0xFFFFFFFF
                    if aarsize == 3: # 64-bit
                         dmi_mem[DMI_DATA1] = (read_val_64 >> 32) & 0xFFFFFFFF
                    else: # 32-bit or less
                         dmi_mem[DMI_DATA1] = 0 # Clear DATA1 for non-64bit reads

            # Update ABSTRACTCS with status and clear busy bit
            update_cmderr(cmderr)
            return

        else: # transfer == 0
            # Handle non-transfer commands if necessary (e.g., just execute from progbuf)
            print("  Abstract CMD: AccessReg: non-transfer command ignored")
            update_cmderr(0) # No error, but nothing done here
            return

    else: # Unknown command type
        print(f"  Abstract CMD: Unsupported command type {command_type}")
        update_cmderr(7) # Command not supported error
        return

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"DMI Server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                buffer = b''
                try:
                    while True:
                        # Read enough data for the longest possible message (WRITE)
                        # Header (6 bytes) + Data (4 bytes) = 10 bytes
                        # Or just Header (6 bytes) for READ
                        chunk = conn.recv(1024)
                        if not chunk:
                            print("Client disconnected.")
                            break
                        buffer += chunk
                        print(f"Received raw chunk: {chunk.hex()}, Buffer now: {buffer.hex()}")

                        # Process complete messages from the buffer
                        while True:
                            if len(buffer) < 6:
                                # Not enough data for even the header
                                break

                            # Peek at the header
                            command, address, data_length = struct.unpack(">BIB", buffer[:6])
                            print(f"Processing msg: Cmd={command}, Addr=0x{address:02X}, Len={data_length}")

                            if command == READ_COMMAND:
                                print(f"  Handling READ command for Addr=0x{address:04X}. (Ignoring data_length={data_length})")
                                handle_dmi_read(address, conn)
                                # Consume the header from the buffer
                                buffer = buffer[6:]
                                print(f"  READ processed. Buffer remaining: {buffer.hex()}")

                            elif command == WRITE_COMMAND:
                                expected_len = 6 + 4 # Header + 32-bit data
                                if len(buffer) < expected_len:
                                    # Not enough data for the full write command yet
                                    print(f"  Waiting for more data for WRITE (need {expected_len}, have {len(buffer)})")
                                    break # Go back to recv more data

                                # Extract data
                                write_data = struct.unpack(">I", buffer[6:10])[0]
                                # Execute the write
                                handle_dmi_write(address, write_data, conn)
                                # Consume the message from the buffer
                                buffer = buffer[expected_len:]
                                print(f"  WRITE processed. Buffer remaining: {buffer.hex()}")

                            else:
                                print(f"Error: Invalid command received: {command}")
                                # Consume the header (or potentially more if fixed size assumed)
                                # and try to continue, or close connection
                                buffer = buffer[6:] # Basic recovery: assume header size
                                # conn.sendall(struct.pack(">BI", RESPONSE_ERROR, 0)) # Example

                except ConnectionResetError:
                    print("Client connection reset.")
                except Exception as e:
                    print(f"An error occurred: {e}")
                finally:
                    print(f"Connection from {addr} closed.")
                    buffer = b'' # Clear buffer for next connection

if __name__ == "__main__":
    main()
