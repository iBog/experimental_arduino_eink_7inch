const express = require('express');
const puppeteer = require('puppeteer');
const Jimp = require('jimp');
const sharp = require('sharp');
const fs = require('fs');
const path = require('path');

const app = express();
app.use(express.json()); // Support JSON-encoded bodies
app.use(express.text({ type: 'text/html', limit: '1mb' }));

const CONFIG_PATH = path.join(__dirname, 'config.json');

// Default configuration
const DEFAULT_CONFIG = {
  mode: 'demo',
  url: null,
  removeClasses: [],
  mobileMode: false,
  dismissCookies: false,
  timestampWatermark: false,
  format: 'bmp',
  resizeAlgorithm: 'lanczos3',
  sharpen: 0,
  bwrDither: false,
  viewport: { width: 800, height: 480, layoutWidth: 800 },
  crop: { x: 0, y: 0, width: 800, height: 480 }
};

// User agents
const DESKTOP_UA = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115 Safari/537.36';
const MOBILE_UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Mobile/15E148 Safari/604.1';

// Helper to load config
function loadConfig() {
  try {
    if (fs.existsSync(CONFIG_PATH)) {
      const saved = JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8'));
      // Merge with defaults to ensure all fields exist
      return { ...DEFAULT_CONFIG, ...saved };
    }
  } catch (e) {
    console.error("Error loading config:", e);
  }
  return { ...DEFAULT_CONFIG };
}

// Initialize config file if it doesn't exist
function initConfig() {
  if (!fs.existsSync(CONFIG_PATH)) {
    console.log('Creating default config.json');
    fs.writeFileSync(CONFIG_PATH, JSON.stringify(DEFAULT_CONFIG, null, 2));
  }
}

initConfig();

// Helper to remove elements by class names
async function removeElementsByClasses(page, classNames) {
    if (!classNames || classNames.length === 0) return;
    console.log('Removing elements with classes:', classNames);
    await page.evaluate((classes) => {
        classes.forEach(className => {
            const elements = document.querySelectorAll('.' + className);
            elements.forEach(el => el.remove());
        });
    }, classNames);
}

// Helper to try accepting cookies
async function tryToDismissCookies(page) {
    console.log('Attempting to dismiss cookie banners...');
    try {
        const clicked = await page.evaluate(() => {
            const commonWords = ['accept', 'agree', 'allow', 'принять', 'принять все', 'согласен', 'соглашаюсь', 'ok', 'got it', 'понятно', 'в другой раз'];
            const blackList = ['settings', 'options', 'custom', 'manage', 'more', 'info', 'policy', 'read', 'learn']; // Don't click "Manage Settings"
            
            function isVisible(elem) {
                return !!(elem.offsetWidth || elem.offsetHeight || elem.getClientRects().length);
            }
            
            function checkText(node) {
                const text = node.innerText || node.textContent || "";
                const lower = text.toLowerCase().trim();
                if (!commonWords.some(w => lower.includes(w))) return false;
                if (blackList.some(w => lower.includes(w))) return false;
                return true;
            }

            // Find potential buttons (button, a, div roles, inputs)
            const candidates = Array.from(document.querySelectorAll('button, a, div[role="button"], input[type="button"], input[type="submit"], span[role="button"]'));
            
            const cookieKeywords = ['cookie', 'consent', 'gdpr', 'privacy', 'banner', 'notice'];
            
            let bestCandidate = null;
            let bestScore = 0; // 2 = in cookie container, 1 = just text match

            for (const btn of candidates) {
                if (!isVisible(btn)) continue;
                if (!checkText(btn)) continue;
                
                let score = 1;
                
                // Check matching text length (shorter is usually better "Accept" vs "Accept and read privacy policy")
                const textLen = (btn.innerText || "").length;
                if (textLen > 50) continue; 

                // Check ancestors
                let parent = btn.parentElement;
                while (parent && parent !== document.body) {
                    const attr = (parent.id + " " + parent.className).toLowerCase();
                    if (cookieKeywords.some(k => attr.includes(k))) {
                        score = 2;
                        break;
                    }
                    parent = parent.parentElement;
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestCandidate = btn;
                }
            }

            if (bestCandidate) {
                bestCandidate.click();
                return true;
            }
            return false;
        });

        if (clicked) {
            console.log('Clicked a potential cookie consent button.');
            // Wait a bit for animation/reload
            await new Promise(r => setTimeout(r, 1000));
        } else {
            console.log('No obvious cookie button found.');
        }
    } catch (e) {
        console.error('Error trying to dismiss cookies:', e);
    }
}

// Helper to add timestamp watermark
// cropBounds: optional {x, y, width, height} to position watermark relative to crop area
async function addTimestampWatermark(imagePath, timezoneOffset = 3, cropBounds = null) {
    try {
        console.log(`Adding timestamp watermark with GMT+${timezoneOffset} offset`);
        
        // Load the image
        const image = await Jimp.read(imagePath);
        
        // Get current time in UTC
        const now = new Date();
        // Apply timezone offset (GMT+3 means add 3 hours to UTC)
        const offsetMillis = timezoneOffset * 60 * 60 * 1000;
        const localTime = new Date(now.getTime() + offsetMillis);
        
        // Format date and time (without timezone)
        const dateStr = localTime.toISOString().split('T')[0]; // YYYY-MM-DD
        const timeStr = localTime.toISOString().split('T')[1].split('.')[0]; // HH:MM:SS
        const timestamp = `${dateStr} ${timeStr}`;
        
        // Load a larger/bolder font for e-ink readability
        const font = await Jimp.loadFont(Jimp.FONT_SANS_14_BLACK);
        
        // Calculate text dimensions
        const textWidth = Jimp.measureText(font, timestamp);
        const textHeight = 14; // Fixed height for consistent positioning
        const paddingH = 4; // Horizontal padding
        const paddingV = 2; // Vertical padding
        const boxWidth = textWidth + paddingH * 2;
        const boxHeight = textHeight + paddingV * 2;
        
        let boxX, boxY;
        if (cropBounds) {
            // Position at bottom-left corner of crop area (flush to corner)
            boxX = cropBounds.x;
            boxY = cropBounds.y + cropBounds.height - boxHeight;
        } else {
            // Position at bottom-left corner of full image
            boxX = 0;
            boxY = image.bitmap.height - boxHeight;
        }
        
        // Draw solid white background
        image.scan(boxX, boxY, boxWidth, boxHeight, function(px, py, idx) {
            this.bitmap.data[idx] = 255;     // R
            this.bitmap.data[idx + 1] = 255; // G
            this.bitmap.data[idx + 2] = 255; // B
        });
        
        // Draw timestamp text
        const textX = boxX + paddingH;
        const textY = boxY + paddingV;
        image.print(font, textX, textY, timestamp);
        
        // Save the image
        await image.writeAsync(imagePath);
        console.log('Timestamp watermark added:', timestamp);
    } catch (error) {
        console.error('Error adding timestamp watermark:', error);
        // Don't fail the whole process if watermark fails
    }
}

// Config endpoints
app.get('/config', (req, res) => {
  res.json(loadConfig());
});

app.post('/config', (req, res) => {
  console.log('Received config save request:', req.body);
  try {
    fs.writeFileSync(CONFIG_PATH, JSON.stringify(req.body, null, 2));
    console.log('Config saved successfully to:', CONFIG_PATH);
    res.json({ success: true });
  } catch (e) {
    console.error('Error writing config file:', e);
    res.status(500).json({ error: e.message });
  }
});

app.get('/config.html', (req, res) => {
  res.sendFile(path.join(__dirname, 'html', 'config.html'));
});

// Health check
app.get('/health', (req, res) => res.status(200).send('OK'));

// Preview endpoint
app.get('/preview', async (req, res) => {
  const url = req.query.url;
  const mode = req.query.mode;
  const width = parseInt(req.query.width) || 800;
  const height = parseInt(req.query.height) || 600;
  const layoutWidth = parseInt(req.query.layoutWidth) || width; // Optional layout width
  const dismissCookies = req.query.dismissCookies === 'true';
  const timestampWatermark = req.query.timestampWatermark === 'true';
  const removeClassesParam = req.query.removeClasses;
  const removeClasses = removeClassesParam ? removeClassesParam.split(',').map(s => s.trim()).filter(Boolean) : [];
  const mobileMode = req.query.mobileMode === 'true';

  if (!url && mode !== 'weather' && mode !== 'demo') return res.status(400).send('Missing url parameter');

  try {
    const browser = await puppeteer.launch({
      headless: 'new',
      args: ['--no-sandbox', '--disable-setuid-sandbox']
    });
    const page = await browser.newPage();
    
    // Force scale factor to 1.0
    const deviceScaleFactor = 1.0;
    const viewWidth = layoutWidth > width ? layoutWidth : width;

    await page.setViewport({ 
        width: viewWidth, 
        height: height,
        deviceScaleFactor: deviceScaleFactor
    });
    
    await page.setUserAgent(mobileMode ? MOBILE_UA : DESKTOP_UA);
    
    if (mode === 'weather') {
        console.log(`Previewing weather mode`);
        const indexPath = path.join(__dirname, 'html', 'index.html');
        const contentHtml = fs.readFileSync(indexPath, 'utf8');
        await page.setContent(contentHtml, { waitUntil: 'networkidle0', timeout: 30000 });
        
        try {
            await page.waitForSelector('body.data-loaded', { timeout: 10000 });
        } catch (e) {
            console.warn('Preview timeout waiting for data-loaded');
        }
    } else if (mode === 'demo') {
        console.log(`Previewing demo mode`);
        const demoHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <style>
                body { margin: 0; padding: 0; background: white; font-family: sans-serif; overflow: hidden; width: 800px; height: 480px; position: relative; }
                .rect { position: absolute; border: 2px solid black; }
                .text { position: absolute; font-size: 20px; font-weight: bold; color: black; }
                .center-text { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); color: red; font-size: 40px; font-weight: bold; }
            </style>
        </head>
        <body>
            <!-- 5 Concentric Rectangles -->
            <div class="rect" style="top: 0; left: 0; right: 0; bottom: 0;"></div>
            <div class="rect" style="top: 10px; left: 10px; right: 10px; bottom: 10px;"></div>
            <div class="rect" style="top: 20px; left: 20px; right: 20px; bottom: 20px;"></div>
            <div class="rect" style="top: 30px; left: 30px; right: 30px; bottom: 30px;"></div>
            <div class="rect" style="top: 40px; left: 40px; right: 40px; bottom: 40px;"></div>

            <!-- Directional Labels -->
            <div class="text" style="top: 10px; left: 50%; transform: translateX(-50%);">TOP</div>
            <div class="text" style="bottom: 10px; left: 50%; transform: translateX(-50%);">BOTTOM</div>
            <div class="text" style="top: 50%; left: 10px; transform: translateY(-50%);">LEFT</div>
            <div class="text" style="top: 50%; right: 10px; transform: translateY(-50%);">RIGHT</div>

            <!-- Center Text -->
            <div class="center-text">Hello, World!</div>
        </body>
        </html>`;
        await page.setContent(demoHtml, { waitUntil: 'networkidle0', timeout: 30000 });
    } else {
        console.log(`Previewing: ${url} at ${layoutWidth}x${Math.round(height/deviceScaleFactor)} (Output: ${width}x${height}, Scale: ${deviceScaleFactor})`);
        try {
            await page.goto(url, { waitUntil: 'networkidle2', timeout: 60000 });
        } catch (navError) {
            // If networkidle2 times out, try with just domcontentloaded
            console.warn('Navigation timeout, retrying with domcontentloaded:', navError.message);
            await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
        }
        
        // Try to dismiss cookies if requested
        if (dismissCookies) {
            await tryToDismissCookies(page);
        }
        
        // Remove elements by class names
        if (removeClasses.length > 0) {
            await removeElementsByClasses(page, removeClasses);
        }
    }

    // Wait a bit for dynamic content
    await new Promise(resolve => setTimeout(resolve, 2000));

    const screenshotBuffer = await page.screenshot();
    await browser.close();

    // Add timestamp watermark if enabled
    if (timestampWatermark) {
        try {
            console.log(`Adding timestamp watermark to preview with GMT+3 offset`);
            // Save buffer to temp file
            const tempPath = path.join(__dirname, `preview_temp_${Date.now()}.png`);
            fs.writeFileSync(tempPath, screenshotBuffer);
            
            // Get crop bounds from query params for correct watermark positioning
            const cropX = parseInt(req.query.cropX);
            const cropY = parseInt(req.query.cropY);
            const cropW = parseInt(req.query.cropW);
            const cropH = parseInt(req.query.cropH);
            const cropBounds = (!isNaN(cropX) && !isNaN(cropY) && !isNaN(cropW) && !isNaN(cropH)) 
                ? { x: cropX, y: cropY, width: cropW, height: cropH } 
                : null;
            
            // Add watermark positioned within crop area
            await addTimestampWatermark(tempPath, 3, cropBounds);
            
            // Read back the watermarked image
            const watermarkedBuffer = fs.readFileSync(tempPath);
            fs.unlinkSync(tempPath);
            
            res.set('Content-Type', 'image/png');
            res.send(watermarkedBuffer);
            return;
        } catch (error) {
            console.error('Error adding watermark to preview:', error);
            // Fall through to send original screenshot
        }
    }

    res.set('Content-Type', 'image/png');
    res.send(screenshotBuffer);
  } catch (err) {
    console.error('Preview error:', err);
    res.status(500).send('Preview generation failed: ' + err.message);
  }
});

app.post('/render', async (req, res) => {
  const html = req.body;
  let url = req.query.url;
  const mode = req.query.mode;
  
  // Load config
  const config = loadConfig();
  
  // Determine rendering parameters
  // If no explicit input provided, fall back to config
  const useConfig = !html && !url && !mode;
  
  let effectiveMode = mode;
  if (useConfig) {
      if (config.mode) {
          effectiveMode = config.mode;
      }
      if (config.url && effectiveMode !== 'weather' && effectiveMode !== 'demo') {
          url = config.url;
      }
  }

  // Defaults or overrides
  const width = parseInt(req.query.width) || (useConfig ? config.viewport?.width : 800) || 800;
  const height = parseInt(req.query.height) || (useConfig ? config.viewport?.height : 480) || 480;
  const layoutWidth = parseInt(req.query.layoutWidth) || (useConfig ? config.viewport?.layoutWidth : width) || width;
  const dismissCookies = (req.query.dismissCookies === 'true') || (useConfig ? !!config.dismissCookies : false);
  const timestampWatermark = (req.query.timestampWatermark === 'true') || (useConfig ? !!config.timestampWatermark : false);
  const removeClasses = useConfig ? (config.removeClasses || []) : [];
  const mobileMode = useConfig ? !!config.mobileMode : false;

  // Determine format from query or config
  const formatRaw = req.query.format || (useConfig ? config.format : null) || 'bmp';
  const format = formatRaw.toLowerCase();

  const baseName = `render_${Date.now()}`;
  const pngPath = path.join(__dirname, `${baseName}.png`);
  const outPath = path.join(__dirname, `${baseName}.${format}`);

  console.log(`Rendering with dimensions: ${width}x${height}, layoutWidth: ${layoutWidth}, format: ${format}, mode: ${effectiveMode || (useConfig ? 'config' : 'default')}`);

  try {
    const browser = await puppeteer.launch({
      headless: 'new',
      args: [
        '--no-sandbox',
        '--disable-setuid-sandbox',
        '--disable-web-security',
        '--disable-features=site-per-process'
      ]
    });

    const page = await browser.newPage();
    
    // Force scale factor to 1.0 to ensure predictable 1:1 pixel mapping
    // If layoutWidth is larger, we just set the viewport larger.
    const deviceScaleFactor = 1.0;
    const viewWidth = layoutWidth > width ? layoutWidth : width;
    // const viewHeight = Math.round(height * (viewWidth / width)); // Maintain aspect ratio? 
    // No, simpler: just use the configured height or a larger one if needed. 
    // actually, for the dashboard, we want 800x480 exact.
    
    await page.setViewport({ 
        width: viewWidth, 
        height: height, 
        deviceScaleFactor: deviceScaleFactor 
    });

    await page.setUserAgent(mobileMode ? MOBILE_UA : DESKTOP_UA);

    let contentHtml = null;

    if (effectiveMode === 'weather') {
      console.log(`Using weather mode - loading index.html from server directory`);
      const indexPath = path.join(__dirname, 'html', 'index.html');
      try {
        contentHtml = fs.readFileSync(indexPath, 'utf8');
        console.log(`Successfully loaded index.html`);
      } catch (readError) {
        await browser.close();
        return res.status(500).send(`Ошибка чтения index.html: ${readError.message}`);
      }
      await page.setContent(contentHtml, { waitUntil: 'networkidle0', timeout: 60000 });
    } else if (effectiveMode === 'demo') {
        console.log(`Using demo mode - loading test pattern`);
        const demoHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <style>
                body { margin: 0; padding: 0; background: white; font-family: sans-serif; overflow: hidden; width: 800px; height: 480px; position: relative; }
                .rect { position: absolute; border: 2px solid black; }
                .text { position: absolute; font-size: 20px; font-weight: bold; color: black; }
                .center-text { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); color: red; font-size: 40px; font-weight: bold; }
            </style>
        </head>
        <body>
            <!-- 5 Concentric Rectangles -->
            <div class="rect" style="top: 0; left: 0; right: 0; bottom: 0;"></div>
            <div class="rect" style="top: 10px; left: 10px; right: 10px; bottom: 10px;"></div>
            <div class="rect" style="top: 20px; left: 20px; right: 20px; bottom: 20px;"></div>
            <div class="rect" style="top: 30px; left: 30px; right: 30px; bottom: 30px;"></div>
            <div class="rect" style="top: 40px; left: 40px; right: 40px; bottom: 40px;"></div>

            <!-- Directional Labels -->
            <div class="text" style="top: 10px; left: 50%; transform: translateX(-50%);">TOP</div>
            <div class="text" style="bottom: 10px; left: 50%; transform: translateX(-50%);">BOTTOM</div>
            <div class="text" style="top: 50%; left: 10px; transform: translateY(-50%);">LEFT</div>
            <div class="text" style="top: 50%; right: 10px; transform: translateY(-50%);">RIGHT</div>

            <!-- Center Text -->
            <div class="center-text">Hello, World!</div>
        </body>
        </html>`;
        await page.setContent(demoHtml, { waitUntil: 'networkidle0', timeout: 30000 });
    } else if (url) {
      console.log(`Загрузка URL: ${url}`);
      await page.goto(url, { waitUntil: 'networkidle2', timeout: 60000 });
      
      // Try to dismiss cookies if requested
      if (dismissCookies) {
          await tryToDismissCookies(page);
      }
      
      // Remove elements by class names
      if (removeClasses.length > 0) {
          await removeElementsByClasses(page, removeClasses);
      }
    } else if (html) {
      console.log(`Рендер HTML из тела запроса`);
      await page.setContent(html, { waitUntil: 'networkidle0', timeout: 60000 });
    } else {
      await browser.close();
      return res.status(400).send('Ошибка: передайте HTML в теле запроса, параметр ?url= или заголовок mode=weather (или настройте config.json)');
    }

    if (effectiveMode === 'weather') {
      try {
        console.log('Waiting for data-loaded class...');
        await page.waitForSelector('body.data-loaded', { timeout: 20000 });
        console.log('Data loaded signal received.');
      } catch (e) {
        console.warn('Timeout waiting for data-loaded class, proceeding with screenshot anyway.');
      }
      // Small buffer for final rendering
      await new Promise(resolve => setTimeout(resolve, 1000));
    } else {
      await new Promise(resolve => setTimeout(resolve, 5000)); // задержка перед скриншотом
    }
    
    // Screenshot options with optional cropping
    const screenshotOptions = { path: pngPath };
    if (useConfig && config.crop) {
        screenshotOptions.clip = {
            x: config.crop.x / deviceScaleFactor,
            y: config.crop.y / deviceScaleFactor,
            width: config.crop.width / deviceScaleFactor,
            height: config.crop.height / deviceScaleFactor
        };
        console.log('Applying crop (adjusted for scale):', screenshotOptions.clip);
    }

    await page.screenshot(screenshotOptions);
    await browser.close();

    // Always resize to exactly 800x480
    const OUTPUT_WIDTH = 800;
    const OUTPUT_HEIGHT = 480;
    const resizedPath = path.join(__dirname, `${baseName}_resized.png`);
    
    // Get resize algorithm from config or query
    const resizeAlgorithm = req.query.resizeAlgorithm || (useConfig ? config.resizeAlgorithm : 'lanczos3') || 'lanczos3';
    const validKernels = ['nearest', 'cubic', 'mitchell', 'lanczos2', 'lanczos3'];
    const kernel = validKernels.includes(resizeAlgorithm) ? resizeAlgorithm : 'lanczos3';
    
    // Get sharpen amount (0 = off, 1-3 recommended for e-ink)
    const sharpen = parseFloat(req.query.sharpen) || (useConfig ? config.sharpen : 0) || 0;
    
    console.log(`Resizing cropped image to ${OUTPUT_WIDTH}x${OUTPUT_HEIGHT} using ${kernel} algorithm, sharpen: ${sharpen}`);
    
    let sharpPipeline = sharp(pngPath)
      .resize(OUTPUT_WIDTH, OUTPUT_HEIGHT, { fit: 'fill', kernel: kernel });
    
    // Apply sharpening if enabled (helps text on e-ink)
    if (sharpen > 0) {
      sharpPipeline = sharpPipeline.sharpen({ sigma: sharpen });
    }
    
    await sharpPipeline.toFile(resizedPath);
    fs.unlinkSync(pngPath);
    
    // Add timestamp watermark if enabled
    if (timestampWatermark) {
        await addTimestampWatermark(resizedPath, 3); // GMT+3 as specified
    }
    
    if (format === 'bmp') {
      const image = await Jimp.read(resizedPath);
      // Write BMP as-is, let ESP32 handle orientation
      await image.writeAsync(outPath);
      fs.unlinkSync(resizedPath);
    } else if (format === 'bwr') {
      // Process for GxEPD2 3-color (Black/White/Red) binary format
      // Output: [BlackPlane][RedPlane]
      // Packing: 1 bit per pixel, 8 pixels per byte, MSB first.
      // Logic: 0 = Active (Black or Red), 1 = Inactive (White or No Red)
      
      const bwrDither = (req.query.bwrDither === 'true') || (useConfig ? !!config.bwrDither : false);
      console.log(`BWR conversion with dithering: ${bwrDither}`);
      
      const { data, info } = await sharp(resizedPath)
        .ensureAlpha()
        .raw()
        .toBuffer({ resolveWithObject: true });

      const w = info.width;
      const h = info.height;
      const stride = Math.ceil(w / 8);
      const planeSize = stride * h;
      
      // Initialize buffers with 0xFF (All 1s -> White / No Red)
      const bwBuffer = Buffer.alloc(planeSize, 0xFF);
      const redBuffer = Buffer.alloc(planeSize, 0xFF);
      
      // For dithering, we need float buffers to accumulate error
      const pixels = new Float32Array(w * h * 3); // RGB only
      for (let i = 0; i < w * h; i++) {
        pixels[i * 3] = data[i * 4];
        pixels[i * 3 + 1] = data[i * 4 + 1];
        pixels[i * 3 + 2] = data[i * 4 + 2];
      }
      
      // Floyd-Steinberg dithering distribution
      const distributeError = (x, y, errR, errG, errB) => {
        const offsets = [
          [1, 0, 7/16],
          [-1, 1, 3/16],
          [0, 1, 5/16],
          [1, 1, 1/16]
        ];
        for (const [dx, dy, factor] of offsets) {
          const nx = x + dx;
          const ny = y + dy;
          if (nx >= 0 && nx < w && ny < h) {
            const nidx = (ny * w + nx) * 3;
            pixels[nidx] += errR * factor;
            pixels[nidx + 1] += errG * factor;
            pixels[nidx + 2] += errB * factor;
          }
        }
      };

      for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
          const pidx = (y * w + x) * 3;
          const r = Math.max(0, Math.min(255, pixels[pidx]));
          const g = Math.max(0, Math.min(255, pixels[pidx + 1]));
          const b = Math.max(0, Math.min(255, pixels[pidx + 2]));
          
          // Calculate squared euclidean distance to palette colors
          const distBlack = r*r + g*g + b*b;
          const distWhite = (r-255)**2 + (g-255)**2 + (b-255)**2;
          const distRed   = (r-255)**2 + g*g + b*b;

          let chosenR, chosenG, chosenB;
          let isBlack = false;
          let isRed = false;

          // Determine closest color
          if (distRed < distBlack && distRed < distWhite) {
            isRed = true;
            chosenR = 255; chosenG = 0; chosenB = 0;
          } else if (distBlack <= distWhite) {
            isBlack = true;
            chosenR = 0; chosenG = 0; chosenB = 0;
          } else {
            chosenR = 255; chosenG = 255; chosenB = 255;
          }
          
          // Apply Floyd-Steinberg dithering if enabled
          if (bwrDither) {
            const errR = r - chosenR;
            const errG = g - chosenG;
            const errB = b - chosenB;
            distributeError(x, y, errR, errG, errB);
          }

          const byteIdx = y * stride + Math.floor(x / 8);
          const bitMask = 0x80 >> (x % 8);

          if (isBlack) {
            // Black: BW=0, Red=1
            bwBuffer[byteIdx] &= ~bitMask;
          } else if (isRed) {
            // Red: BW=1, Red=0
            redBuffer[byteIdx] &= ~bitMask;
          }
        }
      }
      
      fs.writeFileSync(outPath, Buffer.concat([bwBuffer, redBuffer]));
      fs.unlinkSync(resizedPath);

    } else if (format === 'png') {
      // Process PNG with sharp
      const colors = parseInt(req.query.colors) || (useConfig ? config.colors : null);
      const dither = (req.query.dither === 'true') || (useConfig ? !!config.dither : false);
      
      const options = { 
        compressionLevel: 6,
        palette: false,
        dither: dither ? 1.0 : 0 // 1.0 = diffusion dither, 0 = no dither
      };
      
      // Only add colors if specified and valid (2-256)
      if (colors && colors >= 2 && colors <= 256) {
        options.palette = true;
        options.colors = colors;
      }
      
      const tempPath = path.join(__dirname, `${baseName}_temp.png`);
      await sharp(resizedPath)
        .toFormat('png', options)
        .toFile(tempPath);
      fs.unlinkSync(resizedPath);
      fs.renameSync(tempPath, outPath);
    } else {
      fs.renameSync(resizedPath, outPath);
    }

    res.sendFile(outPath, () => {
      fs.unlink(outPath, () => {});
    });
  } catch (err) {
    console.error('Ошибка рендера:', err.message);
    res.status(500).send(`Ошибка рендера: ${err.message}`);
  }
});

const PORT = 3123;
app.listen(PORT, () => {
  console.log(`HTML2Image API запущен: http://<ваш-IP>:${PORT}/render`);
});
