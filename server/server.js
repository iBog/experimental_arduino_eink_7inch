const express = require('express');
const puppeteer = require('puppeteer');
const Jimp = require('jimp');
const sharp = require('sharp');
const fs = require('fs');
const path = require('path');

const app = express();
app.use(express.text({ type: 'text/html', limit: '1mb' }));

app.post('/render', async (req, res) => {
  const html = req.body;
  const url = req.query.url;
  const mode = req.query.mode;
  const format = (req.query.format || 'png').toLowerCase(); // png or bmp
  const width = parseInt(req.query.width) || 800; // Default width
  const height = parseInt(req.query.height) || 480; // Default height
  const baseName = `render_${Date.now()}`;
  const pngPath = path.join(__dirname, `${baseName}.png`);
  const outPath = path.join(__dirname, `${baseName}.${format}`);

  console.log(`Rendering with dimensions: ${width}x${height}, format: ${format}, mode: ${mode || 'default'}`);

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
    await page.setViewport({ width: width, height: height });

    await page.setUserAgent(
      'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115 Safari/537.36'
    );

    let contentHtml = null;

    if (mode === 'weather') {
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
    } else if (url) {
      console.log(`Загрузка URL: ${url}`);
      await page.goto(url, { waitUntil: 'networkidle2', timeout: 60000 });
    } else if (html) {
      console.log(`Рендер HTML из тела запроса`);
      await page.setContent(html, { waitUntil: 'networkidle0', timeout: 60000 });
    } else {
      await browser.close();
      return res.status(400).send('Ошибка: передайте HTML в теле запроса, параметр ?url= или заголовок mode=weather');
    }

    await new Promise(resolve => setTimeout(resolve, 4000)); // задержка перед скриншотом
    await page.screenshot({ path: pngPath });
    await browser.close();

    if (format === 'bmp') {
      const image = await Jimp.read(pngPath);
      // Write BMP as-is, let ESP32 handle orientation
      await image.writeAsync(outPath);
      fs.unlinkSync(pngPath);
    } else if (format === 'png') {
      // Process PNG with sharp
      const colors = parseInt(req.query.colors);
      const options = { 
        compressionLevel: 6,
        palette: false
      };
      
      // Only add colors if specified and valid (2-256)
      if (colors >= 2 && colors <= 256) {
        options.palette = true;
        options.colors = colors;
      }
      
      const tempPath = path.join(__dirname, `${baseName}_temp.png`);
      await sharp(pngPath)
        .toFormat('png', options)
        .toFile(tempPath);
      fs.unlinkSync(pngPath);
      fs.renameSync(tempPath, outPath);
    } else {
      fs.renameSync(pngPath, outPath);
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
