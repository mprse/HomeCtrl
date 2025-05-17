#!/bin/bash

# filepath: /home/bibi/src/HomeCtrl/build.sh

CONFIG_DIR="./config"
PATCH_FILE="./freertos_smp.patch" # Path to the freertos_smp.patch file
PATCH_APPLIED_FLAG="./patch_applied.flag" # File to track if the patch has been applied

# Function to apply the freertos_smp.patch
apply_patch() {
    if [ -f "$PATCH_FILE" ]; then
        echo "Checking if freertos_smp.patch is already applied..."
        # Check if the patch has already been applied using grep
        if grep -q "specific_unique_line_from_patch" ./portable/ThirdParty/GCC/RP2040/library.cmake; then
            echo "freertos_smp.patch already applied. Skipping."
        else
            echo "Applying freertos_smp.patch..."
            # Navigate to the correct directory before applying the patch
            PATCH_DIR="./FreeRTOS-Kernel"
            if [ -d "$PATCH_DIR" ]; then
                pushd "$PATCH_DIR" > /dev/null
                if patch -p1 < "../freertos_smp.patch"; then
                    popd > /dev/null
                    touch "$PATCH_APPLIED_FLAG"
                    echo "freertos_smp.patch applied successfully."
                else
                    popd > /dev/null
                    echo "Failed to apply freertos_smp.patch. Please check for conflicts."
                    exit 1
                fi
            else
                echo "Error: Directory $PATCH_DIR does not exist."
                exit 1
            fi
        fi
    else
        echo "Patch file not found: $PATCH_FILE"
        exit 1
    fi
}

# Check if a project name is provided
if [ "$#" -lt 1 ]; then
    echo "Usage: ./build.sh <project_name> [--apply-patch]"
    echo "Available projects:"
    for dir in "$CONFIG_DIR"/*/; do
        echo "  - $(basename "$dir")"
    done
    exit 1
fi

PROJECT=$1
PROJECT_CONFIG_DIR="$CONFIG_DIR/$PROJECT"
MAIN_DIR="./"

# Check for the --apply-patch option
if [[ "$2" == "--apply-patch" ]]; then
    apply_patch
fi

# Validate the project name
if [ ! -d "$PROJECT_CONFIG_DIR" ]; then
    echo "Error: Configuration directory for project '$PROJECT' does not exist."
    exit 1
fi

# Copy configuration files
echo "Copying configuration files for project '$PROJECT'..."
cp "$PROJECT_CONFIG_DIR/CMakeLists.txt" "$MAIN_DIR"
cp "$PROJECT_CONFIG_DIR/config.h" "$MAIN_DIR"

# Create and navigate to the build directory
BUILD_DIR="./build"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi
cd "$BUILD_DIR"

# Run CMake and Make
echo "Building project '$PROJECT'..."
cmake -DPICO_BOARD=pico_w -DPICO_SDK_PATH=/home/bibi/src/HomeCtrl/pico-sdk ..
make

# Check if the build was successful
if [ $? -eq 0 ]; then
    echo "Build completed successfully for project '$PROJECT'."
else
    echo "Build failed for project '$PROJECT'."
    exit 1
fi
