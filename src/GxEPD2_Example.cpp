#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/TimesNRCyr12.h>
#include <GxEPD2_3C.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>

// Render API configuration
// const char* renderApiUrl = "http://192.168.2.139:3123/render?format=bmp&width=100&height=100";
// const char* renderApiUrl = "http://192.168.2.139:3123/render?format=bmp&url=https://www.onliner.by";
// const char* renderApiUrl = "http://192.168.2.139:3123/render?format=png&width=100&height=100";
// const char* renderApiUrl = "http://192.168.2.139:3123/render?url=https://mediametrics.ru/rating/ru&format=png&width=800&height=480";
// const char* renderApiUrl = "http://192.168.2.139:3123/render?url=https://www.bbc.com&format=bmp&contrast=1";
// const char* renderApiUrl = "http://192.168.2.139:3123/render?mode=weather&format=bmp&width=800&height=478";
const char* renderApiUrl = "http://192.168.2.139:3123/render";

// WiFi credentials
const char* ssid = "bogswifi5";
const char* password = "bog12345";

// Error handling and retry configuration
const int MAX_RETRY_ATTEMPTS = 3;
const int RETRY_DELAY_MS = 2000; // 2 seconds between retries
const int HTTP_TIMEOUT_MS = 60000; // 60 second timeout
const char* CACHED_IMAGE_FILENAME = "/cached.bmp"; // Fallback cached file

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

// Buffer declarations for image display
static const uint16_t max_row_width = 1448; // for up to 6\" display 1448x1072
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits

// Function declarations
bool renderAndDownloadImage(const String& htmlContent, const char* filename, bool enableCaching = 1);
bool downloadImage(const String& url, const String& htmlContent, const char* filename);
bool downloadImageWithRetry(const String& url, const String& htmlContent, const char* filename);
bool copyFile(const char* source, const char* destination);
bool fileExists(const char* filename);
void displayImage(const char* filename, int16_t x, int16_t y);
void displayBMP(const char* filename, int16_t x, int16_t y);
void displayErrorScreen(const char* title, const char* message);
void connectWiFi();
void printBMPInfo(const char* filename);
void listDir(const char* dirname, uint8_t levels);
uint16_t read16(File& f);
uint32_t read32(File& f);

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
        
        // List all files to see what's taking up space
        listDir("/", 0);

        // Cleanup legacy files to ensure sufficient space
        const char* filesToDelete[] = {"/converted.bmp", "/cached.img", "/cached.png", "/rendered.png"};
        for (const char* f : filesToDelete) {
            if (SPIFFS.exists(f)) {
                Serial.printf("Deleting legacy file to free space: %s\n", f);
                SPIFFS.remove(f);
            }
        }
        
        Serial.printf("SPIFFS Free after cleanup: %d bytes\n", SPIFFS.totalBytes() - SPIFFS.usedBytes());
    }

    connectWiFi();

    // Render HTML to image and download it
    const char* imageFilename = "/rendered.bmp";

    // Read HTML content from file dynamically
    String htmlContent = "";

    // Test with caching enabled (default) and disabled
    bool imageDownloaded = renderAndDownloadImage(htmlContent, imageFilename); // Default: caching enabled (1)
    // bool imageDownloaded = renderAndDownloadImage(htmlContent, imageFilename, 0); // Example: caching disabled (0)

    // Display the image on e-ink display (can be disabled for debugging)
    bool displayEnabled = true; // Set to false to disable display for debugging
    if (displayEnabled) {
        Serial.println("Initializing e-paper display...");

        // Add delay before display initialization to ensure power stabilization
        delay(1000);

        // Initialize display with longer timeout and reset
        Serial.println("Resetting display...");
        uint32_t dt = millis();
        display.init(115200, true, 30, false); // 30 second timeout, reset=true
        Serial.printf("Display initialized in %lu ms\n", millis() - dt);

        display.setRotation(0);
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&TimesNRCyr12pt8b);

        if (imageDownloaded && displayEnabled) {
            // Display the image
            displayImage(imageFilename, 0, 0);
            // Update the display after all writeImage calls are complete
            uint32_t dtRefresh = millis();
            display.display(false); // false = full update
            Serial.printf("Full display refresh completed in %lu ms\n", millis() - dtRefresh);
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
    Serial.println("Entering deep sleep for 1 hours...");
    esp_sleep_enable_timer_wakeup(1 * 60 * 60 * 1000000ULL); // 1 hours in microseconds
    esp_deep_sleep_start();
}

void loop()
{
    // Empty loop - everything happens in setup() due to deep sleep
}

// ==== Function Implementations ====

void connectWiFi()
{
    uint32_t start = millis();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("WiFi connected in %lu ms\n", millis() - start);
}

// Main function to render HTML to image and download it
bool renderAndDownloadImage(const String& htmlContent, const char* filename, bool enableCaching)
{
    Serial.println("Attempting to download image from render API...");
    Serial.printf("Caching %s\n", enableCaching ? "enabled" : "disabled");

    // Try to download with retry logic
    bool success = downloadImageWithRetry(renderApiUrl, htmlContent, filename);

    if (success) {
        Serial.println("Image download successful");
        // Cache the successful download for future fallback if caching is enabled
        if (enableCaching) {
            Serial.println("Caching successful download...");
            copyFile(filename, CACHED_IMAGE_FILENAME);
        } else {
            Serial.println("Caching disabled - skipping cache copy");
        }
        return true;
    } else {
        Serial.println("All download attempts failed, checking for cached file...");

        // Try to use cached file as fallback only if caching is enabled
        if (enableCaching && fileExists(CACHED_IMAGE_FILENAME)) {
            Serial.println("Using cached image file as fallback");
            if (copyFile(CACHED_IMAGE_FILENAME, filename)) {
                return true;
            }
        } else if (!enableCaching) {
            Serial.println("Caching disabled - no fallback available");
        } else {
            Serial.println("No cached file available, download failed");
        }

        return false;
    }
}

// Function to download image from render API
bool downloadImage(const String& url, const String& htmlContent, const char* filename)
{
    uint32_t tStart = millis();
    Serial.println("Downloading image...");
    Serial.println("URL: " + String(url));
    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode;
    // Use POST for render API with HTML content, GET for direct URLs
    // Add headers to prevent cookies and ensure clean request
    http.addHeader("Content-Type", "text/html; charset=utf-8");
    http.addHeader("Cookie", ""); // Clear any existing cookies
    http.addHeader("Cache-Control", "no-cache");
    http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115 Safari/537.36");
    
    // Additional headers to prevent tracking and cookies
    http.addHeader("Accept", "*/*");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("Connection", "close");
    
    uint32_t tReq = millis();
    httpCode = http.POST(htmlContent);
    Serial.printf("HTTP Request completed in %lu ms\n", millis() - tReq);
    Serial.println("Using POST method with cookie prevention");

    Serial.printf("HTTP response code: %d\n", httpCode);

    if (httpCode == 200) {
        // Get the image data
        int contentLength = http.getSize();
        Serial.println("Http content length: " + String(contentLength) + " bytes");
        
        if (contentLength > 0) {
            // Check for free space and cleanup if necessary
            size_t spiffsTotalBytes = SPIFFS.totalBytes();
            size_t spiffsUsedBytes = SPIFFS.usedBytes();
            size_t spiffsFreeBytes = spiffsTotalBytes - spiffsUsedBytes;
            
            Serial.printf("SPIFFS Free: %d bytes, Required: %d bytes\n", spiffsFreeBytes, contentLength);

            if (spiffsFreeBytes < contentLength) {
                Serial.println("Insufficient space. Attempting cleanup...");
                
                // Delete the target file if it exists (it might be overwritten anyway, but removing it frees space calculation)
                if (SPIFFS.exists(filename)) {
                    Serial.printf("Removing existing target file: %s\n", filename);
                    SPIFFS.remove(filename);
                    spiffsFreeBytes = spiffsTotalBytes - SPIFFS.usedBytes();
                }

                // If still not enough, delete the cache
                if (spiffsFreeBytes < contentLength) {
                    if (SPIFFS.exists(CACHED_IMAGE_FILENAME)) {
                        Serial.printf("Removing cached file to free space: %s\n", CACHED_IMAGE_FILENAME);
                        SPIFFS.remove(CACHED_IMAGE_FILENAME);
                        spiffsFreeBytes = spiffsTotalBytes - SPIFFS.usedBytes();
                    }
                }
                
                Serial.printf("SPIFFS Free after cleanup: %d bytes\n", spiffsFreeBytes);
            }

            // Always remove the file before writing to ensure fresh start
            if (SPIFFS.exists(filename)) {
                SPIFFS.remove(filename);
            }

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
                Serial.printf("SPIFFS Total: %d bytes\n", spiffsTotalBytes);
                Serial.printf("SPIFFS Used: %d bytes\n", spiffsUsedBytes);
                Serial.printf("SPIFFS Free: %d bytes\n", spiffsTotalBytes - spiffsUsedBytes);
                Serial.printf("Content length: %d bytes\n", contentLength);

                http.end();
                return false;
            }

            // Get the stream and write to file
            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            int bytesRead = 0;
            int totalBytes = 0;

            uint32_t tDownload = millis();
            while (http.connected() && (totalBytes < contentLength)) {
                bytesRead = stream->readBytes(buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    file.write(buffer, bytesRead);
                    totalBytes += bytesRead;
                }
                // delay(1); // Removed to improve speed
            }
            Serial.printf("Stream download and write to SPIFFS in %lu ms\n", millis() - tDownload);

            file.close();
            Serial.printf("Image downloaded successfully: %d bytes\n", totalBytes);
            http.end();
            Serial.printf("Total downloadImage duration: %lu ms\n", millis() - tStart);
            return true;
        }
    } else {
        Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    Serial.printf("Total downloadImage duration (failed): %lu ms\n", millis() - tStart);
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
    } else {
        Serial.printf("Unknown or unsupported image format: 0x%02X 0x%02X 0x%02X 0x%02X\n",
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

            // Process each row - handle both top-down and bottom-up BMPs
            for (int16_t fileRow = startRow; fileRow < endRow; fileRow++) {
                // Calculate the correct file row position based on BMP orientation
                int16_t actualFileRow;
                if (topDown) {
                    // Top-down: fileRow 0 is top of image
                    actualFileRow = fileRow;
                } else {
                    // Bottom-up: fileRow 0 is bottom of image, so we need to read from bottom
                    actualFileRow = height - 1 - fileRow;
                }

                file.seek(imageOffset + actualFileRow * rowSize + startCol * 3);

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

                // Calculate display row - now fileRow correctly represents the display row
                int16_t displayRow = offsetY + fileRow;
                display.writeImage(output_row_mono_buffer, output_row_color_buffer,
                    offsetX + startCol, displayRow, visibleWidth, 1);
            }

            Serial.printf("BMP loaded and drawn in %lu ms\n", millis() - startTime);
        } else {
            Serial.printf("Unsupported BMP format: %d-bit, height=%d\n", depth, height);
        }
    } else {
        Serial.println("Invalid BMP signature");
    }

    file.close();
}

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
    uint32_t tStart = millis();
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

    uint8_t buffer[4096];
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
    Serial.printf("File copy completed in %lu ms\n", millis() - tStart);
    return true;
}

void listDir(const char* dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = SPIFFS.open(dirname);
    if (!root) {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
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
    display.print(renderApiUrl);

    Serial.println("Error screen displayed");
}
