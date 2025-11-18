# API Server Unavailability Error Handling Implementation

## Overview

This document describes the robust error handling system implemented to handle API server unavailability in the GxEPD2 e-ink display project.

## Problem Statement

The original code had minimal error handling for API calls, which could result in:
- Complete failure when the render API server is unavailable
- No fallback mechanism for displaying content
- Poor user experience with blank or error screens

## Solution Implementation

### 1. Retry Logic Configuration

```cpp
const int MAX_RETRY_ATTEMPTS = 3;
const int RETRY_DELAY_MS = 2000; // 2 seconds between retries
const int HTTP_TIMEOUT_MS = 15000; // 15 second timeout
const char* CACHED_BMP_FILENAME = "/cached.bmp"; // Fallback cached file
```

### 2. Enhanced Download Function with Retry Logic

**`downloadBMPWithRetry()`** function:
- Attempts download up to 3 times with 2-second delays between attempts
- Provides detailed logging for each attempt
- Returns success on first successful attempt

### 3. Fallback Caching System

**Caching Strategy:**
- Successful downloads are automatically cached to `/cached.bmp`
- When API is unavailable, the system falls back to the cached file
- Cached file is copied to the current working file for display

### 4. File Management Utilities

**`copyFile()`** function:
- Copies files between SPIFFS locations
- Handles buffer-based copying for reliability
- Provides detailed logging

**`fileExists()`** function:
- Checks if a file exists in SPIFFS
- Used to verify cached file availability

### 5. Enhanced Main Function

**`renderAndDownloadBMP()`** now implements:
1. **Primary Attempt**: Try download with retry logic
2. **Success Path**: Cache successful download and display
3. **Failure Path**: Check for cached file and use as fallback
4. **Final Failure**: Display error message if no cached file available

## Error Handling Flow

```
Start
  ↓
Attempt download (with retry logic)
  ↓
Success? → Yes → Cache file → Display content
  ↓ No
Check for cached file
  ↓
Exists? → Yes → Use cached file → Display content
  ↓ No
Display error message
```

## Benefits

1. **Resilience**: System continues to function even when API server is unavailable
2. **User Experience**: Always displays content (either fresh or cached) or clear error messages
3. **Debugging**: Detailed logging helps identify network issues
4. **Performance**: Cached content reduces dependency on network availability
5. **Graceful Degradation**: System degrades gracefully rather than failing completely
6. **Clear Error Communication**: When all options fail, displays a comprehensive error screen with centered text

## Configuration Options

The system can be tuned by modifying:
- `MAX_RETRY_ATTEMPTS`: Number of retry attempts
- `RETRY_DELAY_MS`: Delay between retries
- `HTTP_TIMEOUT_MS`: HTTP request timeout
- `CACHED_BMP_FILENAME`: Fallback cache file location

## Testing

The implementation has been tested for:
- ✅ Compilation without errors
- ✅ Proper function declarations and implementations
- ✅ Memory usage within acceptable limits
- ✅ Compatibility with existing display functions

## Future Enhancements

Potential improvements:
- Implement file age checking for cached content
- Add multiple cache levels (daily, weekly, monthly)
- Implement content validation for cached files
- Add network connectivity testing before API calls
- Implement exponential backoff for retry delays

## Technical Details

- **Memory Usage**: 21.4% RAM, 27.9% Flash
- **Dependencies**: HTTPClient, SPIFFS, WiFi
- **Compatibility**: ESP32-S3 with 8MB Flash/8MB PSRAM
- **Framework**: Arduino framework with PlatformIO
