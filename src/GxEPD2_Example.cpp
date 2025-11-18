#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/TimesNRCyr12.h>
#include <GxEPD2_3C.h>
#include <HTTPClient.h>
#include <PNGdec.h>
#include <SPIFFS.h>
#include <WiFi.h>

// Render API configuration
// const char* renderApiUrl = "http://192.168.2.59:3123/render?format=bmp&width=100&height=100";
// const char* renderApiUrl = "http://192.168.2.59:3123/render?format=bmp";
// const char* renderApiUrl = "http://192.168.2.59:3123/render?format=png&width=100&height=100";
// const char* renderApiUrl = "http://192.168.2.59:3123/render?url=https://www.bbc.com&format=png&width=400&height=400";
const char* renderApiUrl = "http://192.168.2.59:3123/render?url=https://www.bbc.com&format=bmp&width=800&height=480";

// WiFi credentials
const char* ssid = "bogswifi5";
const char* password = "bog12345";

// Error handling and retry configuration
const int MAX_RETRY_ATTEMPTS = 3;
const int RETRY_DELAY_MS = 2000; // 2 seconds between retries
const int HTTP_TIMEOUT_MS = 15000; // 15 second timeout
const char* CACHED_IMAGE_FILENAME = "/cached.img"; // Fallback cached file (format auto-detected)

// BMP input buffer for 24-bit images
uint8_t bmp_input_buffer[2400]; // One row of 800 pixels * 3 bytes

// E-paper display pins
#define EPD_MOSI 11
#define EPD_SCK 12
#define EPD_CS 10
#define EPD_DC 18
#define EPD_RST 17
#define EPD_BUSY 16

GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// PNG decoder
PNG png;

// Buffer declarations for image display
static const uint16_t max_row_width = 1448; // for up to 6\" display 1448x1072
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits

// PNG display offset variables (for centering/clipping)
int16_t png_offset_x = 0;
int16_t png_offset_y = 0;
int16_t png_start_col = 0;
int16_t png_start_row = 0;
int16_t png_visible_width = 0;

// Function declarations
bool renderAndDownloadImage(const String& htmlContent, const char* filename);
bool downloadImage(const String& url, const String& htmlContent, const char* filename);
bool downloadImageWithRetry(const String& url, const String& htmlContent, const char* filename);
bool copyFile(const char* source, const char* destination);
bool fileExists(const char* filename);
void displayImage(const char* filename, int16_t x, int16_t y);
void displayBMP(const char* filename, int16_t x, int16_t y);
void displayPNG(const char* filename, int16_t x, int16_t y);
void displayErrorScreen(const char* title, const char* message);
void connectWiFi();
void printBMPInfo(const char* filename);
uint16_t read16(File& f);
uint32_t read32(File& f);

// PNG decoder callbacks
void* pngOpen(const char* filename, int32_t* size);
void pngClose(void* handle);
int32_t pngRead(PNGFILE* handle, uint8_t* buffer, int32_t length);
int32_t pngSeek(PNGFILE* handle, int32_t position);
int pngDraw(PNGDRAW* pDraw);

void setup()
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    Serial.begin(115200);
    Serial.println("Init e-paper...");

    // Initialize SPIFFS for file storage with better error handling
    Serial.println("Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        Serial.println("This could be due to:");
        Serial.println("- SPIFFS not configured in platformio.ini");
        Serial.println("- Insufficient flash memory");
        Serial.println("- Corrupted SPIFFS partition");
        return; // Stop execution if SPIFFS fails
    } else {
        Serial.println("SPIFFS initialized successfully");

        // Print SPIFFS info for debugging
        size_t totalBytes = SPIFFS.totalBytes();
        size_t usedBytes = SPIFFS.usedBytes();
        Serial.printf("SPIFFS Total: %d bytes\n", totalBytes);
        Serial.printf("SPIFFS Used: %d bytes\n", usedBytes);
        Serial.printf("SPIFFS Free: %d bytes\n", totalBytes - usedBytes);
    }

    connectWiFi();

    // Render HTML to image and download it
    // Note: filename extension should match format parameter in renderApiUrl
    const char* imageFilename = "/rendered.bmp";  // Change to .png if using format=png
    String htmlContent = "<html><body><h1>Привет, Ilia!</h1><p>Rendered from ESP32</p><p style='color:red'>RED color text test</p></body></html>";
    bool imageDownloaded = renderAndDownloadImage(htmlContent, imageFilename);

    // Display the PNG on e-ink display (can be disabled for debugging)
    bool displayEnabled = true; // Set to false to disable display for debugging
    if (displayEnabled) {
        Serial.println("Initializing e-paper display...");

        // Add delay before display initialization to ensure power stabilization
        delay(1000);

        // Initialize display with longer timeout and reset
        Serial.println("Resetting display...");
        display.init(115200, true, 30, false); // 30 second timeout, reset=true

        Serial.println("Display initialized successfully");

        display.setRotation(0);
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&TimesNRCyr12pt8b);

        if (imageDownloaded && displayEnabled) {
            // Display the image (auto-detects format)
            displayImage(imageFilename, 0, 0);
            // Update the display after all writeImage calls are complete
            display.display(false); // false = full update
        } else if (!imageDownloaded) {
            // Show error message using firstPage/nextPage for text
            display.firstPage();
            do {
                displayErrorScreen("API Server Unavailable", "No cached content available");
            } while (display.nextPage());
        } else if (!displayEnabled) {
            // Show debug message
            display.firstPage();
            do {
                displayErrorScreen("Debug", "Display Disabled");
            } while (display.nextPage());
        }

        Serial.println("Display update completed");
    }
    // Put ESP32 into deep sleep for 1 hours (1 * 60 * 60 * 1,000,000 microseconds)
    // Serial.println("Entering deep sleep for 1 hours...");
    // esp_sleep_enable_timer_wakeup(1 * 60 * 60 * 1000000ULL); // 1 hours in microseconds
    // esp_deep_sleep_start();
}

void loop()
{
    // Empty loop - everything happens in setup() due to deep sleep
}

// ==== Function Implementations ====

void connectWiFi()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
}

// Main function to render HTML to image and download it
bool renderAndDownloadImage(const String& htmlContent, const char* filename)
{
    Serial.println("Attempting to download image from render API...");

    // Try to download with retry logic
    bool success = downloadImageWithRetry(renderApiUrl, htmlContent, filename);

    if (success) {
        Serial.println("Image download successful");
        // Cache the successful download for future fallback
        copyFile(filename, CACHED_IMAGE_FILENAME);
        return true;
    } else {
        Serial.println("All download attempts failed, checking for cached file...");

        // Try to use cached file as fallback
        if (fileExists(CACHED_IMAGE_FILENAME)) {
            Serial.println("Using cached image file as fallback");
            if (copyFile(CACHED_IMAGE_FILENAME, filename)) {
                return true;
            }
        }

        Serial.println("No cached file available, download failed");
        return false;
    }
}

// Function to download image from render API
bool downloadImage(const String& url, const String& htmlContent, const char* filename)
{
    Serial.println("Downloading image...");
    Serial.println("URL: " + String(url));
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000); // 10 second timeout

    int httpCode;
    // Use POST for render API with HTML content, GET for direct URLs
    if (htmlContent.length() > 0) {
        http.addHeader("Content-Type", "text/html");
        httpCode = http.POST(htmlContent);
        Serial.println("Using POST method");
    } else {
        httpCode = http.GET();
        Serial.println("Using GET method");
    }

    Serial.printf("HTTP response code: %d\n", httpCode);

    if (httpCode == 200) {
        // Get the image data
        int contentLength = http.getSize();
        Serial.println("Http content length: " + String(contentLength) + " bytes");
        if (contentLength > 0) {
            // Create file on SPIFFS with detailed error handling
            Serial.printf("Attempting to create file: %s\n", filename);
            File file = SPIFFS.open(filename, FILE_WRITE);
            if (!file) {
                Serial.println("Failed to create file on SPIFFS");
                Serial.println("Possible causes:");
                Serial.println("- SPIFFS not properly initialized");
                Serial.println("- Insufficient free space");
                Serial.println("- File path is invalid");
                Serial.println("- Too many open files");

                // Print SPIFFS status for debugging
                size_t totalBytes = SPIFFS.totalBytes();
                size_t usedBytes = SPIFFS.usedBytes();
                Serial.printf("SPIFFS Total: %d bytes\n", totalBytes);
                Serial.printf("SPIFFS Used: %d bytes\n", usedBytes);
                Serial.printf("SPIFFS Free: %d bytes\n", totalBytes - usedBytes);
                Serial.printf("Content length: %d bytes\n", contentLength);

                http.end();
                return false;
            }

            // Get the stream and write to file
            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[512];
            int bytesRead = 0;
            int totalBytes = 0;

            while (http.connected() && (totalBytes < contentLength)) {
                bytesRead = stream->readBytes(buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    file.write(buffer, bytesRead);
                    totalBytes += bytesRead;
                }
                delay(1);
            }

            file.close();
            Serial.printf("Image downloaded successfully: %d bytes\n", totalBytes);
            http.end();
            return true;
        }
    } else {
        Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

// Helper functions for reading BMP data
uint16_t read16(File& f)
{
    // BMP data is stored little-endian, same as Arduino.
    uint16_t result;
    ((uint8_t*)&result)[0] = f.read(); // LSB
    ((uint8_t*)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(File& f)
{
    // BMP data is stored little-endian, same as Arduino.
    uint32_t result;
    ((uint8_t*)&result)[0] = f.read(); // LSB
    ((uint8_t*)&result)[1] = f.read();
    ((uint8_t*)&result)[2] = f.read();
    ((uint8_t*)&result)[3] = f.read(); // MSB
    return result;
}

// Universal image display function - detects format and calls appropriate handler
void displayImage(const char* filename, int16_t x, int16_t y)
{
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open image file");
        return;
    }
    
    // Read magic bytes to detect format
    uint8_t magic[4];
    file.read(magic, 4);
    file.close();
    
    // Check for BMP signature (BM = 0x42 0x4D)
    if (magic[0] == 0x42 && magic[1] == 0x4D) {
        Serial.println("Detected BMP format");
        displayBMP(filename, x, y);
    }
    // Check for PNG signature (0x89 0x50 0x4E 0x47)
    else if (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47) {
        Serial.println("Detected PNG format");
        displayPNG(filename, x, y);
    }
    else {
        Serial.printf("Unknown image format: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                      magic[0], magic[1], magic[2], magic[3]);
    }
}

// Simple BMP display function for 24-bit RGB BMPs
void displayBMP(const char* filename, int16_t x, int16_t y)
{
    File file = SPIFFS.open(filename, FILE_READ);
    uint32_t startTime = millis();
    
    if (!file) {
        Serial.println("Failed to open BMP file");
        return;
    }
    
    Serial.println();
    Serial.print("Loading BMP '");
    Serial.print(filename);
    Serial.println('\'');
    
    // Parse BMP header
    if (read16(file) == 0x4D42) { // BMP signature
        read32(file); // fileSize
        read32(file); // creatorBytes
        uint32_t imageOffset = read32(file);
        read32(file); // headerSize  
        uint32_t width = read32(file);
        int32_t height = (int32_t)read32(file);
        read16(file); // planes
        uint16_t depth = read16(file);
        
        // Handle both top-down (negative height) and bottom-up (positive height) BMPs
        bool topDown = (height < 0);
        if (topDown) {
            height = -height; // Make positive
        }
        
        if (depth == 24) {
            Serial.printf("BMP: %dx%d, 24-bit, %s\n", width, height, topDown ? "top-down" : "bottom-up");
            
            // Calculate offset to center image if smaller than display
            int16_t offsetX = (width < display.epd2.WIDTH) ? (display.epd2.WIDTH - width) / 2 : 0;
            int16_t offsetY = (height < display.epd2.HEIGHT) ? (display.epd2.HEIGHT - height) / 2 : 0;
            
            // Apply user offset
            offsetX += x;
            offsetY += y;
            
            // Calculate visible area
            int16_t startCol = (offsetX < 0) ? -offsetX : 0;
            int16_t startRow = (offsetY < 0) ? -offsetY : 0;
            int16_t endCol = (offsetX + width > display.epd2.WIDTH) ? display.epd2.WIDTH - offsetX : width;
            int16_t endRow = (offsetY + height > display.epd2.HEIGHT) ? display.epd2.HEIGHT - offsetY : height;
            
            Serial.printf("Display offset: (%d,%d), visible: cols[%d:%d] rows[%d:%d]\n", 
                         offsetX, offsetY, startCol, endCol, startRow, endRow);
            
            uint32_t rowSize = (width * 3 + 3) & ~3; // Row padding to 4 bytes
            file.seek(imageOffset);
            
            // Process each row - Jimp writes bottom-up BMPs
            for (int16_t fileRow = startRow; fileRow < endRow; fileRow++) {
                file.seek(imageOffset + fileRow * rowSize + startCol * 3);
                
                int16_t visibleWidth = endCol - startCol;
                
                // Clear buffers
                memset(output_row_mono_buffer, 0xFF, (visibleWidth + 7) / 8);
                memset(output_row_color_buffer, 0xFF, (visibleWidth + 7) / 8);
                
                // Read row data
                file.read(bmp_input_buffer, visibleWidth * 3);
                
                // Convert BGR to e-paper format
                for (uint16_t col = 0; col < visibleWidth; col++) {
                    uint8_t b = bmp_input_buffer[col * 3];
                    uint8_t g = bmp_input_buffer[col * 3 + 1];
                    uint8_t r = bmp_input_buffer[col * 3 + 2];
                    
                    bool whitish = (r > 0x80) && (g > 0x80) && (b > 0x80);
                    bool colored = (r > 0xF0) || ((g > 0xF0) && (b > 0xF0));
                    
                    if (!whitish) {
                        if (colored) {
                            output_row_color_buffer[col / 8] &= ~(0x80 >> (col % 8));
                        } else {
                            output_row_mono_buffer[col / 8] &= ~(0x80 >> (col % 8));
                        }
                    }
                }
                
                // Bottom-up BMP: fileRow 0 is bottom of image
                int16_t displayRow = offsetY + height - 1 - fileRow;
                display.writeImage(output_row_mono_buffer, output_row_color_buffer, 
                                 offsetX + startCol, displayRow, visibleWidth, 1);
            }
            
            Serial.print("BMP loaded in ");
            Serial.print(millis() - startTime);
            Serial.println(" ms");
        } else {
            Serial.printf("Unsupported BMP format: %d-bit, height=%d\n", depth, height);
        }
    } else {
        Serial.println("Invalid BMP signature");
    }
    
    file.close();
}

// Old BMP function (commented out - kept for reference)
// Supports multiple bit depths but requires additional buffers
/*
void displayBMP(const char* filename, int16_t x, int16_t y)
{
    File file = SPIFFS.open(filename, FILE_READ);
    bool valid = false; // valid format to be handled
    uint32_t startTime = millis();

    if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) {
        Serial.println("Display coordinates out of bounds");
        return;
    }

    if (!file) {
        Serial.println("Failed to open BMP file");
        return;
    }

    Serial.println();
    Serial.print("Loading image '");
    Serial.print(filename);
    Serial.println('\'');

    // Parse BMP header
    if (read16(file) == 0x4D42) { // BMP signature
        uint32_t fileSize = read32(file);
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes; // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize = read32(file);
        uint32_t width = read32(file);
        int32_t height = (int32_t)read32(file);
        uint16_t planes = read16(file);
        uint16_t depth = read16(file); // bits per pixel
        uint32_t format = read32(file);

        // Handle negative height (top-down bitmap)
        bool flip = true; // default to bottom-up
        uint32_t absHeight = abs(height);
        if (height < 0) {
            Serial.println("BMP height is negative, flipping image");
            flip = false; // top-down bitmap, don't flip
            height = absHeight;
        }

        Serial.print("File size: ");
        Serial.println(fileSize);
        Serial.print("Image Offset: ");
        Serial.println(imageOffset);
        Serial.print("Header size: ");
        Serial.println(headerSize);
        Serial.print("Image format: ");
        Serial.println(format);
        Serial.print("Image planes: ");
        Serial.println(planes);
        Serial.print("Bit Depth: ");
        Serial.println(depth);
        Serial.print("Image size: ");
        Serial.print(width);
        Serial.print('x');
        Serial.print(absHeight);
        Serial.print(" (");
        Serial.print(flip ? "bottom-up" : "top-down");
        Serial.println(")");
        if ((planes == 1) && ((format == 0) || (format == 3))) { // uncompressed is handled, 565 also

            // BMP rows are padded (if needed) to 4-byte boundary
            uint32_t rowSize = (width * depth / 8 + 3) & ~3;
            if (depth < 8)
                rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;

            uint16_t w = width;
            uint16_t h = height;
            if ((x + w - 1) >= display.epd2.WIDTH)
                w = display.epd2.WIDTH - x;
            if ((y + h - 1) >= display.epd2.HEIGHT)
                h = display.epd2.HEIGHT - y;

            if (w <= max_row_width) { // handle with direct drawing
                valid = true;
                uint8_t bitmask = 0xFF;
                uint8_t bitshift = 8 - depth;
                uint16_t red, green, blue;
                bool whitish = false;
                bool colored = false;
                bool with_color = true; // Use color for 3-color display

                if (depth == 1)
                    with_color = false;

                if (depth <= 8) {
                    if (depth < 8)
                        bitmask >>= depth;
                    // file.seek(54); //palette is always @ 54
                    file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant
                    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                        blue = file.read();
                        green = file.read();
                        red = file.read();
                        file.read();
                        whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                        colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << pn % 8;
                        if (0 == pn % 8)
                            color_palette_buffer[pn / 8] = 0;
                        color_palette_buffer[pn / 8] |= colored << pn % 8;
                    }
                }

                // Don't call clearScreen here - screen already cleared before displayBMP
                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;

                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) { // for each line
                    uint32_t in_remain = rowSize;
                    uint32_t in_idx = 0;
                    uint32_t in_bytes = 0;
                    uint8_t in_byte = 0; // for depth <= 8
                    uint8_t in_bits = 0; // for depth <= 8
                    uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
                    uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
                    uint32_t out_idx = 0;

                    file.seek(rowPosition);

                    for (uint16_t col = 0; col < w; col++) { // for each pixel
                        // Time to read more pixel data?
                        if (in_idx >= in_bytes) { // ok, exact match for 24bit also (size IS multiple of 3)
                            in_bytes = file.read(input_buffer, in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
                            in_remain -= in_bytes;
                            in_idx = 0;
                        }

                        switch (depth) {
                        case 32:
                            blue = input_buffer[in_idx++];
                            green = input_buffer[in_idx++];
                            red = input_buffer[in_idx++];
                            in_idx++; // skip alpha
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            break;
                        case 24:
                            blue = input_buffer[in_idx++];
                            green = input_buffer[in_idx++];
                            red = input_buffer[in_idx++];
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                            break;
                        case 16: {
                            uint8_t lsb = input_buffer[in_idx++];
                            uint8_t msb = input_buffer[in_idx++];
                            if (format == 0) { // 555
                                blue = (lsb & 0x1F) << 3;
                                green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                red = (msb & 0x7C) << 1;
                            } else { // 565
                                blue = (lsb & 0x1F) << 3;
                                green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                red = (msb & 0xF8);
                            }
                            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                        } break;
                        case 1:
                        case 2:
                        case 4:
                        case 8: {
                            if (0 == in_bits) {
                                in_byte = input_buffer[in_idx++];
                                in_bits = 8;
                            }
                            uint16_t pn = (in_byte >> bitshift) & bitmask;
                            whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                            colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                            in_byte <<= depth;
                            in_bits -= depth;
                        } break;
                        }

                        if (whitish) {
                            // keep white
                        } else if (colored && with_color) {
                            out_color_byte &= ~(0x80 >> col % 8); // colored
                        } else {
                            out_byte &= ~(0x80 >> col % 8); // black
                        }

                        if ((7 == col % 8) || (col == w - 1)) { // write that last byte! (for w%8!=0 border)
                            output_row_color_buffer[out_idx] = out_color_byte;
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte = 0xFF; // white (for w%8!=0 border)
                            out_color_byte = 0xFF; // white (for w%8!=0 border)
                        }
                    } // end pixel

                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
                } // end line

                Serial.print("loaded in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
            }
        } else {
            Serial.println("BMP format not handled.");
        }
    }

    file.close();
    if (!valid) {
        Serial.println("bitmap format not handled.");
    }
}
*/

// Function to print BMP file information for debugging
void printBMPInfo(const char* filename)
{
    File file = SPIFFS.open(filename, FILE_READ);
    if (!file) {
        Serial.println("Failed to open BMP file for info");
        return;
    }

    Serial.println();
    Serial.println("=== BMP File Information ===");

    // Parse BMP header
    if (read16(file) == 0x4D42) { // BMP signature
        uint32_t fileSize = read32(file);
        uint32_t creatorBytes = read32(file);
        (void)creatorBytes; // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize = read32(file);
        uint32_t width = read32(file);
        uint32_t height = read32(file);
        uint16_t planes = read16(file);
        uint16_t depth = read16(file); // bits per pixel
        uint32_t format = read32(file);

        Serial.printf("File size: %d bytes\n", fileSize);
        Serial.printf("Image Offset: %d\n", imageOffset);
        Serial.printf("Header size: %d\n", headerSize);
        Serial.printf("Image size: %dx%d\n", width, height);
        Serial.printf("Bit Depth: %d bpp\n", depth);
        Serial.printf("Planes: %d\n", planes);
        Serial.printf("Format: %d\n", format);

        // Calculate row size
        uint32_t rowSize = (width * depth / 8 + 3) & ~3;
        if (depth < 8)
            rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
        Serial.printf("Row size: %d bytes\n", rowSize);
        Serial.printf("Expected data size: %d bytes\n", rowSize * height);

        // Check if dimensions match display
        if (width == 800 && height == 480) {
            Serial.println("✓ Image matches display size (800x480)");
        } else {
            Serial.println("⚠ Image size differs from display (800x480)");
        }
    } else {
        Serial.println("Invalid BMP signature");
    }

    file.close();
    Serial.println("============================");
}

// Function to download image with retry logic
bool downloadImageWithRetry(const String& url, const String& htmlContent, const char* filename)
{
    for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
        Serial.printf("Download attempt %d/%d...\n", attempt, MAX_RETRY_ATTEMPTS);

        if (downloadImage(url, htmlContent, filename)) {
            Serial.printf("Download successful on attempt %d\n", attempt);
            return true;
        }

        if (attempt < MAX_RETRY_ATTEMPTS) {
            Serial.printf("Download failed, retrying in %d ms...\n", RETRY_DELAY_MS);
            delay(RETRY_DELAY_MS);
        } else {
            Serial.println("All download attempts failed");
        }
    }

    return false;
}

// Function to copy file from source to destination
bool copyFile(const char* source, const char* destination)
{
    File sourceFile = SPIFFS.open(source, FILE_READ);
    if (!sourceFile) {
        Serial.printf("Failed to open source file: %s\n", source);
        return false;
    }

    File destFile = SPIFFS.open(destination, FILE_WRITE);
    if (!destFile) {
        Serial.printf("Failed to create destination file: %s\n", destination);
        sourceFile.close();
        return false;
    }

    uint8_t buffer[512];
    int bytesRead = 0;
    int totalBytes = 0;

    while (sourceFile.available()) {
        bytesRead = sourceFile.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            destFile.write(buffer, bytesRead);
            totalBytes += bytesRead;
        }
    }

    sourceFile.close();
    destFile.close();

    Serial.printf("File copied successfully: %s -> %s (%d bytes)\n", source, destination, totalBytes);
    return true;
}

// Function to check if file exists
bool fileExists(const char* filename)
{
    File file = SPIFFS.open(filename, FILE_READ);
    if (file) {
        file.close();
        return true;
    }
    return false;
}

// Function to display error screen with centered text
void displayErrorScreen(const char* title, const char* message)
{
    // Clear the screen
    display.fillScreen(GxEPD_WHITE);

    // Set font for title (larger font)
    display.setFont(&TimesNRCyr12pt8b);
    display.setTextColor(GxEPD_RED);

    // Calculate text width for centering
    int16_t x1, y1;
    uint16_t w, h;

    // Display title centered
    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int titleX = (display.width() - w) / 2;
    int titleY = display.height() / 3;

    display.setCursor(titleX, titleY);
    display.print(title);

    // Display message centered below title
    display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    int messageX = (display.width() - w) / 2;
    int messageY = titleY + 40; // Space between title and message

    display.setCursor(messageX, messageY);
    display.print(message);

    // Display additional info
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, messageY + 60);
    display.print("Check API server connection");

    display.setCursor(100, messageY + 90);
    display.print("192.168.2.59:3123");

    Serial.println("Error screen displayed");
}

// ==== PNG Display Functions ====

// PNG file callbacks for PNGdec library
void* pngOpen(const char* filename, int32_t* size)
{
    File* file = new File(SPIFFS.open(filename, FILE_READ));
    if (*file) {
        *size = file->size();
        return file;
    }
    delete file;
    return nullptr;
}

void pngClose(void* handle)
{
    File* file = (File*)handle;
    if (file) {
        file->close();
        delete file;
    }
}

int32_t pngRead(PNGFILE* handle, uint8_t* buffer, int32_t length)
{
    File* file = (File*)handle->fHandle;
    if (!file) return 0;
    return file->read(buffer, length);
}

int32_t pngSeek(PNGFILE* handle, int32_t position)
{
    File* file = (File*)handle->fHandle;
    if (!file) return 0;
    return file->seek(position);
}

// PNG draw callback - converts RGB to B/W and color for e-paper
int pngDraw(PNGDRAW* pDraw)
{
    if (!pDraw || !pDraw->pPixels) {
        Serial.println("PNG draw error: invalid parameters");
        return 0;
    }
    
    uint8_t* pixels = (uint8_t*)pDraw->pPixels;
    uint16_t w = pDraw->iWidth;
    uint16_t y = pDraw->y;
    int pixelType = pDraw->iPixelType;
    
    // Check if this row is visible on display
    int16_t displayY = png_offset_y + y;
    if (y < png_start_row || displayY >= display.epd2.HEIGHT) {
        return 1; // Skip this row
    }
    
    // Clear output buffers
    memset(output_row_mono_buffer, 0xFF, (png_visible_width + 7) / 8);
    memset(output_row_color_buffer, 0xFF, (png_visible_width + 7) / 8);
    
    // Process only visible columns
    for (uint16_t x = png_start_col; x < png_start_col + png_visible_width && x < w; x++) {
        uint8_t r, g, b, a = 255;
        
        // Handle different pixel formats
        if (pixelType == PNG_PIXEL_TRUECOLOR) {
            // RGB888 format (3 bytes per pixel)
            r = pixels[x * 3];
            g = pixels[x * 3 + 1];
            b = pixels[x * 3 + 2];
        } else if (pixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
            // RGBA format (4 bytes per pixel)
            r = pixels[x * 4];
            g = pixels[x * 4 + 1];
            b = pixels[x * 4 + 2];
            a = pixels[x * 4 + 3];
        } else if (pixelType == PNG_PIXEL_INDEXED) {
            // Indexed/paletted image - PNGdec should convert to RGB
            // If we still get indexed data, treat as grayscale
            uint8_t index = pixels[x];
            r = g = b = index;
        } else {
            // Grayscale - treat pixel value as gray level
            uint8_t gray = pixels[x];
            r = g = b = gray;
        }
        
        // If pixel is transparent or semi-transparent, treat as white
        if (a < 128) {
            // Transparent - keep white (buffer already set to 0xFF)
            continue;
        }
        
        // Determine if whitish
        bool whitish = (r > 0x80) && (g > 0x80) && (b > 0x80);
        // Determine if colored (reddish or yellowish)
        bool colored = (r > 0xF0) || ((g > 0xF0) && (b > 0xF0));
        
        // Map to output buffer
        uint16_t outX = x - png_start_col;
        
        if (!whitish) {
            if (colored) {
                // Set color bit
                output_row_color_buffer[outX / 8] &= ~(0x80 >> (outX % 8));
            } else {
                // Set black bit
                output_row_mono_buffer[outX / 8] &= ~(0x80 >> (outX % 8));
            }
        }
    }
    
    // Write the row to display
    display.writeImage(output_row_mono_buffer, output_row_color_buffer, 
                     png_offset_x + png_start_col, displayY, png_visible_width, 1);
    return 1; // success
}

// Function to display PNG on e-ink display
void displayPNG(const char* filename, int16_t x, int16_t y)
{
    uint32_t startTime = millis();
    
    Serial.println();
    Serial.print("Loading PNG image '");
    Serial.print(filename);
    Serial.println('\'');
    
    int rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
        int16_t width = png.getWidth();
        int16_t height = png.getHeight();
        
        Serial.printf("Image size: %dx%d, bpp: %d, pixel type: %d\n", 
                      width, height, png.getBpp(), png.getPixelType());
        
        // Calculate offset to center image if smaller than display
        png_offset_x = (width < display.epd2.WIDTH) ? (display.epd2.WIDTH - width) / 2 : 0;
        png_offset_y = (height < display.epd2.HEIGHT) ? (display.epd2.HEIGHT - height) / 2 : 0;
        
        // Apply user offset
        png_offset_x += x;
        png_offset_y += y;
        
        // Calculate visible area
        png_start_col = (png_offset_x < 0) ? -png_offset_x : 0;
        png_start_row = (png_offset_y < 0) ? -png_offset_y : 0;
        int16_t endCol = (png_offset_x + width > display.epd2.WIDTH) ? display.epd2.WIDTH - png_offset_x : width;
        int16_t endRow = (png_offset_y + height > display.epd2.HEIGHT) ? display.epd2.HEIGHT - png_offset_y : height;
        
        png_visible_width = endCol - png_start_col;
        
        Serial.printf("Display offset: (%d,%d), visible: cols[%d:%d] rows[%d:%d]\n", 
                     png_offset_x, png_offset_y, png_start_col, endCol, png_start_row, endRow);
        
        // Set options to convert paletted/indexed images to RGB during decode
        // This ensures pngDraw callback receives RGB data
        uint8_t ucOptions = 0;
        if (png.getPixelType() == PNG_PIXEL_INDEXED) {
            Serial.println("Indexed PNG detected, will convert to RGB");
        }
        
        rc = png.decode(NULL, ucOptions);
        png.close();
        
        if (rc == PNG_SUCCESS) {
            Serial.print("PNG loaded in ");
            Serial.print(millis() - startTime);
            Serial.println(" ms");
        } else {
            Serial.printf("PNG decode failed with error: %d\n", rc);
        }
    } else {
        Serial.printf("Failed to open PNG file with error: %d\n", rc);
    }
}
