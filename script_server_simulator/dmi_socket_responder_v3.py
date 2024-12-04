import socket
import struct
import time
# import ipdb
import binascii
import threading
import traceback # Import traceback for detailed errors

HOST = 'localhost'
PORT = 5555

READ_COMMAND = 0x00
WRITE_COMMAND = 0x01

RESPONSE_OK = 0x00
RESPONSE_ERROR = 0x01

XLEN = 64
misa = 0x800000000014112d
NUM_GPRS = 32
gprs = [0] * NUM_GPRS
dcsr = 0x40000003
dpc = 0
mstatus = 0xA00000200
vlenb = 16 if (misa >> 21) & 1 else 0
mtopi = 0

# --- DMI Registers ---
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

# --- CSR Numbers ---
CSR_MISA = 0x301
CSR_MSTATUS = 0x300
CSR_DCSR = 0x7B0
CSR_DPC = 0x7B1
CSR_VLENB = 0xC22
CSR_MTOPI = 0x350
GPR_BASE = 0x1000

reset_in_progress = False

dmi_mem = {
    DMI_DMCONTROL: 0x0,
    DMI_DMSTATUS: (1 << 8) | (1 << 9) | (1 << 7) | 0x2, # 0x00000382
    DMI_HARTINFO: 0x00102004,
    DMI_ABSTRACTCS: (0 << 8) | (0 << 12) | (2 << 0) | (2 << 24),
    DMI_COMMAND: 0x00000000,
    DMI_ABSTRACTAUTO: 0x00000000,
    DMI_DATA0: 0x00000000,
    DMI_DATA1: 0x00000000,
    DMI_PROGBUF0: 0x00000000,
    DMI_SBCS: 0x00000000,
}

def decode_abstract_command(cmd):
    cmdtype = (cmd >> 24) & 0xff
    if cmdtype == 0:
        regno = cmd & 0xffff
        write = (cmd >> 16) & 1
        transfer = (cmd >> 17) & 1
        postexec = (cmd >> 18) & 1
        aarsize = (cmd >> 20) & 0x7
        print(f"  --> AccessReg: reg=0x{regno:04x}, write={write}, transfer={transfer}, postexec={postexec}, size={aarsize}")
    else:
        print(f"  --> Unsupported command type: {cmdtype}")

def update_cmderr(error_code):
    global dmi_mem
    preserved_bits = dmi_mem[DMI_ABSTRACTCS] & (0xFFF | (0x1F << 24))
    dmi_mem[DMI_ABSTRACTCS] = preserved_bits | ((error_code & 0x7) << 8)
    dmi_mem[DMI_ABSTRACTCS] &= ~(1 << 12)
def decode_abstract_command(cmd):
    cmdtype = (cmd >> 24) & 0xff
    if cmdtype == 0:
        regno = cmd & 0xffff
        write = (cmd >> 16) & 1
        transfer = (cmd >> 17) & 1
        postexec = (cmd >> 18) & 1
        aarsize = (cmd >> 20) & 0x7
        print(f"  --> AccessReg: reg=0x{regno:04x}, write={write}, transfer={transfer}, postexec={postexec}, size={aarsize}")
    else:
        print(f"  --> Unsupported command type: {cmdtype}")
def has_v_extension():
    return (misa >> 21) & 1

def has_smtopi_extension():
    return False

def handle_dmi_read(address, conn):
    print(f"READ from DMI address 0x{address:02X}") 
    """Handles DMI read requests."""
    global dmi_mem, reset_in_progress
    data = dmi_mem.get(address, 0)
    final_response_data = data 
    # Specific handling
    if address == DMI_DMCONTROL:
        print(f"\n===> DMCONTROL READ requested")
        print(f"  Current simulated value: 0x{dmi_mem[DMI_DMCONTROL]:08X}")
        print(f"  reset_in_progress = {reset_in_progress}")
        if reset_in_progress:
            print("  -> In reset: clearing DMCONTROL")
            dmi_mem[DMI_DMCONTROL] = 0x0
            reset_in_progress = False
            final_response_data = 0x0
        else:
            final_response_data = dmi_mem[DMI_DMCONTROL] # Read normally if not in reset

        print(f"DMI Read : Addr=0x{address:02X}, Responding with Data=0x{final_response_data:08X}")

    elif address == DMI_DMSTATUS:
        original_data = dmi_mem.get(address, 0)
        final_response_data = (original_data & ~0xF) | 0x2 # Force version 0.13
        print(f"DMI Read : Addr=0x{address:02X}, Original Data=0x{original_data:08X}, Adjusted Data=0x{final_response_data:08X}")
        if final_response_data != original_data:
             dmi_mem[address] = final_response_data # Update stored value ONLY if adjusted

    elif address == DMI_ABSTRACTCS:
        print(f"DMI Read : Addr=0x{address:02X}, Data=0x{final_response_data:08X}") # Use final_response_data
        print(f"  DMI_ABSTRACTCS read: 0x{final_response_data:08X}")

    elif address == DMI_DATA0 or address == DMI_DATA1:
        print(f"DMI Read : Addr=0x{address:02X}, Data=0x{final_response_data:08X}") # Use final_response_data
        print(f"  DMI_DATA{address-DMI_DATA0} read: 0x{final_response_data:08X}")
    else:
         print(f"DMI Read : Addr=0x{address:02X}, Data=0x{final_response_data:08X}") # Use final_response_data

    # Send response back
    try:
        response = struct.pack(">B", RESPONSE_OK) + struct.pack(">I", final_response_data)
        if address == DMI_DMCONTROL and not reset_in_progress: # Check if we just handled reset
            print(f"  <<< Sending DMCONTROL reset ack response bytes: {response.hex()}")
        elif address == DMI_DMCONTROL:
            print(f"  <<< Sending DMCONTROL non-reset response bytes: {response.hex()}")
        conn.sendall(response)
    except Exception as e:
        print(f"  !!!! EXCEPTION during DMI read response sending: {e}")
        # Consider closing connection or handling otherwise
    print(f"  Returning DMCONTROL: 0x{final_response_data:08X}")
    return final_response_data # Return the value that was sent

def handle_dmi_write(address, data, conn):
    print(f"WRITE to DMI address 0x{address:02X}, data=0x{data:08X}")
    """Handles DMI write requests."""
    global dmi_mem, reset_in_progress
    print(f"DMI Write: Addr=0x{address:04X}, Data=0x{data:08X}")
    error_occurred = False

    try:
        if address == DMI_DMCONTROL:
            print(f"\n===> DMCONTROL WRITE requested")
            print(f"  Data being written: 0x{data:08X}")
            if data == 0x0:
                print("  -> Hard reset requested (DMCONTROL = 0), clearing internal state")
                reset_in_progress = True
                dmi_mem[DMI_DMCONTROL] = 0x0
                try:
                    conn.sendall(struct.pack(">B", RESPONSE_OK))
                    print("  -> Sent RESPONSE_OK after hard reset")
                except Exception as e:
                    print(f"  !!!! EXCEPTION while sending response: {e}")
                return

            requested_dmcontrol = data
            print(f"  DMI_DMCONTROL write request: 0x{requested_dmcontrol:08X}")

            ndmreset_req = (requested_dmcontrol >> 1) & 1
            if ndmreset_req:
                print("  -> ndmreset requested, marking reset_in_progress")
                reset_in_progress = True
                print("  -> ndmreset requested, entering reset state")
                # Clear DMACTIVE while entering reset
                dmi_mem[DMI_DMCONTROL] = requested_dmcontrol & ~(1 << 0)
                # dmi_mem[DMI_DMCONTROL] = requested_dmcontrol

                status = dmi_mem.get(DMI_DMSTATUS, 0)
                status &= ~((1 << 9) | (1 << 8) | (1 << 11) | (1 << 10))
                status &= ~(1 << 7)
                status = (status & ~0xF) | 0x2
                dmi_mem[DMI_DMSTATUS] = status

                print(f"  Simulated DMI_DMCONTROL state stored: 0x{dmi_mem[DMI_DMCONTROL]:08X}")
                print(f"  Updated DMI_DMSTATUS for reset: 0x{status:08X}")

            else: # Not an ndmreset request
                if reset_in_progress:
                    print("  !! Still in reset_in_progress, ignoring new DMCONTROL write to avoid overwriting cleared state")
                    return 
                current_dmcontrol = dmi_mem.get(DMI_DMCONTROL, 0)
                was_active = (current_dmcontrol & 1) != 0
                new_dmcontrol_state = requested_dmcontrol
                requesting_active = (requested_dmcontrol & 1) != 0

                if was_active or requesting_active:
                    new_dmcontrol_state |= 1
                    print("  -> Ensuring dmactive=1")
                elif not was_active and not requesting_active:
                   new_dmcontrol_state |= 1
                   print("  -> First write is 0, forcing dmactive=1")

                dmi_mem[DMI_DMCONTROL] = new_dmcontrol_state
                print(f"  Simulated DMI_DMCONTROL state stored: 0x{dmi_mem[DMI_DMCONTROL]:08X}")

                status = dmi_mem.get(DMI_DMSTATUS, 0)
                status &= ~((1 << 9) | (1 << 11) | (1 << 17) | (1 << 19) | (1 << 8) | (1 << 10) | (1 << 16) | (1 << 18))
                haltreq_active = (new_dmcontrol_state >> 31) & 1
                resumereq_active = (new_dmcontrol_state >> 30) & 1
                hartreset_active = (new_dmcontrol_state >> 29) & 1
                ackhavereset_req = (requested_dmcontrol >> 28) & 1
                dm_is_active = (new_dmcontrol_state & 1) == 1

                if haltreq_active: status |= (1 << 9) | (1 << 8)
                elif resumereq_active: status |= (1 << 11) | (1 << 10) | (1 << 17) | (1 << 16)
                elif dm_is_active: status |= (1 << 9) | (1 << 8) # Default halted

                if hartreset_active: status |= (1 << 19) | (1 << 18)
                if ackhavereset_req: status &= ~((1 << 19) | (1 << 18))

                status |= (1 << 7) # authenticated
                status = (status & ~0xF) | 0x2 # version=0.13

                if (status >> 9) & 1: status |= (1 << 8)

                dmi_mem[DMI_DMSTATUS] = status
                print(f"  Updated DMI_DMSTATUS: 0x{status:08X}")

        elif address == DMI_COMMAND:
            print(f"  -> Abstract Command Written: 0x{data:08X}")
            decode_abstract_command(data)
            dmi_mem[address] = data
            dmi_mem[DMI_ABSTRACTCS] |= (1 << 12)
            print(f"  Set Busy. ABSTRACTCS: 0x{dmi_mem[DMI_ABSTRACTCS]:08X}")
            execute_abstract_command(data) # Forwards to function that has globals declared
            print(f"  <- Abstract Command Complete. ABSTRACTCS: 0x{dmi_mem[DMI_ABSTRACTCS]:08X}")
        elif address == DMI_DATA0 or address == DMI_DATA1:
             print(f"  -> DMI_DATA{address-DMI_DATA0} write: 0x{data:08X}")
             dmi_mem[address] = data

        else:
            dmi_mem[address] = data
            print(f"  Stored default write to Addr=0x{address:04X}")

    except Exception as e:
        print(f"  !!!! EXCEPTION during DMI write handling: {e}")
        traceback.print_exc() 
        error_occurred = True

    if not error_occurred:
        print("  -> Sending RESPONSE_OK for WRITE")
        conn.sendall(struct.pack(">B", RESPONSE_OK))
    else:
        print("  -> Skipping RESPONSE_OK due to internal error")
        # Optionally: conn.close() ?

def execute_abstract_command(command):
    """Executes abstract commands (simplified)."""
    global dcsr, dpc, misa, vlenb, mtopi, mstatus, gprs, dmi_mem
    command_type = (command >> 24) & 0xFF

    # Check for Command Type 0: Access Register
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

        if postexec:
            inst = dmi_mem.get(DMI_PROGBUF0, 0)
            print(f"  -> Executing postexec instruction from PROGBUF0: 0x{inst:08x}")

            # Emulate simple lw/ld instruction
            if (inst & 0x7f) == 0x03:  # LOAD
                funct3 = (inst >> 12) & 0x7
                rs1 = (inst >> 15) & 0x1F
                rd = (inst >> 7) & 0x1F
                imm = (inst >> 20) & 0xFFF
                if imm & 0x800:
                    imm |= ~0xFFF  # sign-extend

                addr = gprs[rs1] + imm
                print(f"    Emulated LOAD from addr=0x{addr:x} into x{rd}")

                # Simulate reading from memory (add real backing store if needed)
                mem_val = 0x12345678ABCDEF  # just dummy for now
                if funct3 == 0x3:  # LD
                    val = mem_val
                elif funct3 == 0x2:  # LW
                    val = mem_val & 0xFFFFFFFF
                else:
                    val = 0

                gprs[rd] = val
                print(f"    GPR[{rd}] = 0x{val:x}")
            dmi_mem[DMI_DATA0] = gprs[rd] & 0xFFFFFFFF
            dmi_mem[DMI_DATA1] = (gprs[rd] >> 32) & 0xFFFFFFFF

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
                # Add other writable CSRs if needed (MSTATUS, etc.)
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
                    # Ensure dmi_mem is modified correctly
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
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) # Allow quick restart
        s.bind((HOST, PORT))
        s.listen()
        print(f"DMI Server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            global reset_in_progress
            reset_in_progress = False
            # For now, only resetting the reset flag.
            with conn:
                print(f"Connected by {addr}")
                buffer = b''
                try:
                    while True:
                        chunk = conn.recv(1024)
                        if not chunk:
                            print("Client disconnected.")
                            break
                        buffer += chunk
                        print(f"Received raw chunk: {chunk.hex()}, Buffer now: {buffer.hex()}")

                        # Process complete messages from the buffer
                        while True:
                            if len(buffer) < 6:
                                break

                            command, address, data_length = struct.unpack(">BIB", buffer[:6])
                            print(f"Processing msg: Cmd={command}, Addr=0x{address:02X}, Len={data_length}")

                            if command == READ_COMMAND:
                                print(f"  Handling READ command for Addr=0x{address:04X}. (Ignoring data_length={data_length})")
                                handle_dmi_read(address, conn)
                                buffer = buffer[6:]
                                print(f"  READ processed. Buffer remaining: {buffer.hex()}")

                            elif command == WRITE_COMMAND:
                                expected_len = 6 + 4
                                if len(buffer) < expected_len:
                                    print(f"  Waiting for more data for WRITE (need {expected_len}, have {len(buffer)})")
                                    break

                                write_data = struct.unpack(">I", buffer[6:10])[0]
                                handle_dmi_write(address, write_data, conn)
                                buffer = buffer[expected_len:]
                                print(f"  WRITE processed. Buffer remaining: {buffer.hex()}")

                            else:
                                print(f"Error: Invalid command received: {command}")
                                buffer = buffer[6:]

                except ConnectionResetError:
                    print("Client connection reset.")
                except Exception as e:
                    print(f"An error occurred: {e}")
                    traceback.print_exc() 
                finally:
                    print(f"Connection from {addr} closed.")
                    buffer = b''

if __name__ == "__main__":
    main()