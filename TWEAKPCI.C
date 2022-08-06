#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "int386r.h"


enum
{
    PCI_FUNCTION_ID         = 0xB1,

    // PCI BIOS subfunctions
    PCI_BIOS_PRESENT        = 0x01,
    FIND_PCI_DEVICE         = 0x02,
    FIND_PCI_CLASS_CODE     = 0x03,
    GENERATE_SPECIAL_CYCLE  = 0x06,
    READ_CONFIG_BYTE        = 0x08,
    READ_CONFIG_WORD        = 0x09,
    READ_CONFIG_DWORD       = 0x0A,
    WRITE_CONFIG_BYTE       = 0x0B,
    WRITE_CONFIG_WORD       = 0x0C,
    WRITE_CONFIG_DWORD      = 0x0D,
    GET_IRQ_ROUTING_OPTIONS = 0x0E,
    SET_PCI_IRQ             = 0x0F,

    CFLAG_CARRY             = 0x01,
};


enum Switches
{
    SW_SHOW_BIN     = 0x1,
    SW_SHOW_HEX     = 0x2,
    SW_COMPACT_MODE = 0x4,
};


struct Device
{
    uint8_t bus_nr;
    uint8_t device_and_function_nr;
};


struct Register
{
    uint8_t size;
    uint8_t number;
};


struct Arguments
{
    const char** begin;
    const char** end;
};


enum Switches parse_switch_args(struct Arguments* args)
{
    // Set default values.
    enum Switches sw = SW_SHOW_BIN | SW_SHOW_HEX;
    const char* arg;

    while (args->begin != args->end)
    {
        arg = *args->begin;

        if (arg[0] != '-')
            break;

        if (strlen(arg) != 2)
        {
            printf("Invalid switch \"%s\"\n", arg);
            exit(1);
        }

        switch (toupper(arg[1]))
        {
            case 'B': sw |= SW_SHOW_BIN; sw &= ~SW_SHOW_HEX; break;
            case 'H': sw |= SW_SHOW_HEX; sw &= ~SW_SHOW_BIN; break;
            case 'C': sw |= SW_COMPACT_MODE; break;
            default:
            printf("Invalid switch \"%s\"\n", arg);
            exit(1);
        }
        args->begin++;
    }
    return sw;
}


void check_pci_bios()
{
    struct Regs regs;
    regs.ah = PCI_FUNCTION_ID;
    regs.al = PCI_BIOS_PRESENT;
    int386(0x1a, &regs, &regs);

    if ((regs.cflag & CFLAG_CARRY) || regs.ah != 0 || regs.edx != *(uint32_t*)"PCI ")
    {
        printf("PCI BIOS v2.0c+ not found.\n");
        exit(1);
    }
}


unsigned long parse_unsigned_integer(const char* name, const char* str, char end_char, const char** str_end, int base, unsigned long max)
{
    char* end;
    unsigned long value = strtoul(str, &end, base);
    if (str == end || value > max || *end != end_char)
    {
        printf("Invalid %s \"%s\"\n", name, str);
        exit(1);
    }

    if (str_end != NULL)
        *str_end = end;

    return value;
}


unsigned long parse_unsigned_integer_arg(const char* name, struct Arguments* args, int base, unsigned long max)
{
    if (args->begin == args->end)
    {
        printf("Missing %s\n", name);
        exit(1);
    }
    return parse_unsigned_integer(name, *args->begin++, '\0', NULL, base, max);
}


struct Device parse_device_args(enum Switches sw, struct Arguments* args)
{
    struct Device device;
    struct Regs in_regs, out_regs;
    in_regs.ah = PCI_FUNCTION_ID;
    in_regs.al = FIND_PCI_DEVICE;
    in_regs.cx = parse_unsigned_integer_arg("device id", args, 16, UINT16_MAX);
    in_regs.dx = parse_unsigned_integer_arg("vendor id", args, 16, UINT16_MAX);
    in_regs.si = 0; // assume device index 0

    printf("Tweaking device: DeviceId 0x%04X, VendorId 0x%04X, DeviceIdx 0x0\n", in_regs.cx, in_regs.dx);

    int386(0x1a, &in_regs, &out_regs);

    if ((out_regs.cflag & 0x1) == 1 && out_regs.ah != 0)
    {
        printf("Failed to find device!\n");
        exit(1);
    }

    device.bus_nr = out_regs.bh;
    device.device_and_function_nr = out_regs.bl;

    if (~sw & SW_COMPACT_MODE)
        printf("Device found: BusNr 0x%02X, DeviceNr 0x%02X, FunctionNr 0x%X\n", device.bus_nr, device.device_and_function_nr >> 3, device.device_and_function_nr & 0x7);

    return device;
}


struct Register parse_register_arg(struct Arguments* args)
{
    struct Register reg;

    // Parse reg size specifier.
    const char* arg = *args->begin;
    switch (toupper(*arg))
    {
        case 'B': reg.size = 1; break;
        case 'W': reg.size = 2; break;
        case 'D': reg.size = 4; break;
        default:
        printf("Invalid register size '%c'\n", *arg);
        exit(1);
    }

    // Parse register number.
    reg.number = parse_unsigned_integer("register number", arg + 1, '\0', NULL, 16, 0xFF);

    if (reg.number % reg.size != 0)
    {
        printf("Unaligned register access '%s'\n", arg);
        exit(1);
    }

    args->begin++;

    return reg;
}


uint32_t read_pci_config_register(struct Device pci_device, struct Register pci_reg)
{
    struct Regs regs;

    regs.ah = PCI_FUNCTION_ID;
    if (pci_reg.size == 1)
        regs.al = READ_CONFIG_BYTE;
    else if (pci_reg.size == 2)
        regs.al = READ_CONFIG_WORD;
    else
        regs.al = READ_CONFIG_DWORD;

    regs.bh = pci_device.bus_nr;
    regs.bl = pci_device.device_and_function_nr;
    regs.di = pci_reg.number;

    int386(0x1a, &regs, &regs);

    if ((regs.cflag & CFLAG_CARRY) || regs.ah != 0)
    {
        printf("Failed to read register 0x%02X.\n", pci_reg.number);
        exit(1);
    }

    return regs.ecx;
}


void write_pci_config_register(struct Device pci_device, struct Register pci_reg, uint32_t reg_value)
{
    struct Regs regs;

    regs.ah = PCI_FUNCTION_ID;
    if (pci_reg.size == 1)
        regs.al = WRITE_CONFIG_BYTE;
    else if (pci_reg.size == 2)
        regs.al = WRITE_CONFIG_WORD;
    else
        regs.al = WRITE_CONFIG_DWORD;

    regs.bh = pci_device.bus_nr;
    regs.bl = pci_device.device_and_function_nr;
    regs.di = pci_reg.number;
    regs.ecx = reg_value;

    int386(0x1a, &regs, &regs);

    if ((regs.cflag & CFLAG_CARRY) || regs.ah != 0)
    {
        printf("Failed to read register 0x%02X.\n", pci_reg.number);
        exit(1);
    }
}


void print_register_value(enum Switches sw, const char* label, uint8_t reg_size, uint32_t reg_value)
{
    printf("%s: ", label);

    // Clear high bits to prevent them from being printed.
    reg_value &= UINT32_MAX >> (32 - 8 * reg_size);

    if (sw & SW_SHOW_HEX)
    {
        char* hex_fmt = "0x%00lX";
        hex_fmt[4] = '0' + reg_size * 2;
        printf(hex_fmt, reg_value);

        if (sw & SW_SHOW_BIN)
            printf("  ");
    }

    if (sw & SW_SHOW_BIN)
    {
        int i, j;
        reg_value <<= (4 - reg_size) * 8;
        for (i = 0; ; i++)
        {
            for (j = 0; j < 4; j++)
            {
                printf(reg_value & ((uint32_t)1 << 31) ? "1" : "0");
                reg_value <<= 1;
            }

            if (i == 2 * reg_size - 1)
                break;

            printf(i % 2 != 0 ? " " : "_");
        }
    }

    printf("\n");
}


int main(int argc, const char* argv[])
{
    struct Arguments args;
    enum Switches sw;
    struct Device device;

    printf("TweakPCI - PCI configuration tool v1.0 by TimmermanV\n");

    if (argc < 2)
    {
        printf( "\n"
                "Usage:\n"
                "tweakpci [switch]... device_id vendor_id [register [bit_changes...]]...\n"
                "switch = -b show binary representation only\n"
                "         -h show hexadecimal representation only\n"
                "         -c compact output, shows final value only\n"
                "device_id = hexadecimal id of the PCI device (required)\n"
                "vendor_id = hexadecimal id of the PCI vendor (required)\n"
                "register = size 'b'/'w'/'d' (for 8/16/32-bit) + hexdec regnr\n"
                "bit_changes = index of lowest bit (decimal) + '=' + binary digits\n"
                "Examples:\n"
                "tweakpci -h 0496 1039 d0\n"
                "tweakpci 0496 1039 b40 0=10 5=1 6=1\n"
                "tweakpci -c -b 0496 1039 b40 2=010 b81 2=010\n"
        );
        exit(0);
    }

    // Store args in struct.
    args.begin = argv + 1;
    args.end = argv + argc;

    check_pci_bios();
    sw = parse_switch_args(&args);
    device = parse_device_args(sw, &args);

    while (args.begin != args.end)
    {
        struct Register reg = parse_register_arg(&args);
        bool has_bit_changes = false;
        uint32_t old_reg_value, reg_value;

        reg_value = read_pci_config_register(device, reg);
        old_reg_value = reg_value;

        while (args.begin != args.end)
        {
            const char* arg;
            const char* begin_bin_value;
            uint8_t bit_idx, bin_digit_count;
            uint32_t bin_value, mask;

            arg = *args.begin;
            if (!isdigit(*arg))
                break;
            
            has_bit_changes = true;
            bit_idx = parse_unsigned_integer("bit index", arg, '=', &arg, 10, reg.size * 8 - 1);

            // Skip '='
            arg++;

            begin_bin_value = arg;
            bin_value = parse_unsigned_integer("binary value", arg, '\0', &arg, 2, UINT32_MAX);
            bin_digit_count = arg - begin_bin_value;

            if (bin_digit_count > reg.size * 8 - bit_idx)
            {
                printf("Too many binary digits '%s'\n", begin_bin_value);
                exit(1);
            }

            // Prepare mask.
            mask = UINT32_MAX;
            mask >>= (32 - bin_digit_count);
            mask <<= bit_idx;

            // Modify bits as requested.
            bin_value <<= bit_idx;
            reg_value &= ~mask;
            reg_value |= bin_value;
        
            args.begin++;
        }
        
        if (~sw & SW_COMPACT_MODE)
        {
            printf("\nRegister 0x%02X (%d-bit)\n", reg.number, reg.size * 8);
            if (has_bit_changes)
            {
                print_register_value(sw, "Cur value", reg.size, old_reg_value);
                print_register_value(sw, "Set value", reg.size, reg_value);
            }
        }

        if (has_bit_changes)
        {
            write_pci_config_register(device, reg, reg_value);
            reg_value = read_pci_config_register(device, reg);
        }
        
        if (sw & SW_COMPACT_MODE)
        {
            char* label = "Register 0x00";
            sprintf(label + 11, "%02X", reg.number);
            print_register_value(sw, label, reg.size, reg_value);
        }
        else
        {
            const char* label = has_bit_changes ? "New value" : "Cur value";
            print_register_value(sw, label, reg.size, reg_value);
        }   
    }

    return 0;
}
