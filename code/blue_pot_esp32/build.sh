#!/bin/bash
# Quick build and flash script for Blue POT ESP32

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"
SERIAL_PORT="${1:-/dev/ttyUSB0}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Blue POT ESP32 Build & Flash Script${NC}"
echo "=================================================="
echo ""

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo -e "${YELLOW}Warning: IDF_PATH not set. Trying to source ESP-IDF...${NC}"
    
    # Try common ESP-IDF installation paths
    if [ -d "$HOME/esp/esp-idf" ]; then
        . "$HOME/esp/esp-idf/export.sh"
        echo -e "${GREEN}Sourced ESP-IDF from $HOME/esp/esp-idf${NC}"
    elif [ -d "/opt/esp-idf" ]; then
        . "/opt/esp-idf/export.sh"
        echo -e "${GREEN}Sourced ESP-IDF from /opt/esp-idf${NC}"
    else
        echo -e "${RED}Error: Could not find ESP-IDF. Please install and set IDF_PATH${NC}"
        exit 1
    fi
fi

echo "Serial port: $SERIAL_PORT"
echo "Project directory: $SCRIPT_DIR"
echo ""

# Parse command line arguments
COMMAND="${2:-build}"

case "$COMMAND" in
    "build")
        echo -e "${GREEN}Building project...${NC}"
        cd "$SCRIPT_DIR"
        idf.py build
        echo -e "${GREEN}Build complete!${NC}"
        echo ""
        echo "Next steps:"
        echo "  Flash:        $0 $SERIAL_PORT flash"
        echo "  Build+Flash:  $0 $SERIAL_PORT build-flash"
        echo "  Monitor:      $0 $SERIAL_PORT monitor"
        ;;
        
    "flash")
        echo -e "${GREEN}Flashing firmware...${NC}"
        cd "$SCRIPT_DIR"
        idf.py -p "$SERIAL_PORT" flash
        echo -e "${GREEN}Flash complete!${NC}"
        ;;
        
    "build-flash")
        echo -e "${GREEN}Building and flashing...${NC}"
        cd "$SCRIPT_DIR"
        idf.py -p "$SERIAL_PORT" build flash
        echo -e "${GREEN}Build and flash complete!${NC}"
        ;;
        
    "monitor")
        echo -e "${GREEN}Starting monitor (Ctrl+] to exit)...${NC}"
        cd "$SCRIPT_DIR"
        idf.py -p "$SERIAL_PORT" monitor
        ;;
        
    "build-flash-monitor")
        echo -e "${GREEN}Building, flashing, and starting monitor...${NC}"
        cd "$SCRIPT_DIR"
        idf.py -p "$SERIAL_PORT" build flash monitor
        ;;
        
    "clean")
        echo -e "${YELLOW}Cleaning build directory...${NC}"
        cd "$SCRIPT_DIR"
        idf.py fullclean
        echo -e "${GREEN}Clean complete!${NC}"
        ;;
        
    "erase-flash")
        echo -e "${RED}Erasing entire ESP32 flash...${NC}"
        esptool.py -p "$SERIAL_PORT" erase_flash
        echo -e "${GREEN}Flash erase complete!${NC}"
        ;;
        
    "menuconfig")
        echo -e "${GREEN}Opening menuconfig...${NC}"
        cd "$SCRIPT_DIR"
        idf.py menuconfig
        ;;
        
    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        echo ""
        echo "Usage: $0 [serial_port] [command]"
        echo ""
        echo "Commands:"
        echo "  build              - Build the project"
        echo "  flash              - Flash to ESP32"
        echo "  build-flash        - Build and flash"
        echo "  monitor            - Monitor serial output"
        echo "  build-flash-monitor- Build, flash, and monitor"
        echo "  clean              - Clean build directory"
        echo "  erase-flash        - Erase entire ESP32 flash"
        echo "  menuconfig         - Configure project options"
        echo ""
        echo "Examples:"
        echo "  $0                           # Build (uses /dev/ttyUSB0)"
        echo "  $0 /dev/ttyUSB0 build-flash  # Build and flash"
        echo "  $0 COM3 monitor              # Monitor on COM3 (Windows)"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Done!${NC}"
