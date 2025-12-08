# html2png-api

[English](#english) | [Русский](#russian)

---

<a name="english"></a>
## English

### Description
A Node.js API service that converts HTML content or URLs into PNG, BMP, or BWR (3-color e-ink) images using Puppeteer. Features include custom output dimensions, color quantization, resize algorithms, sharpening, Floyd-Steinberg dithering for e-ink displays, and a web-based configuration UI.

### Prerequisites
- Node.js (v18 or higher recommended)
- npm
- Debian 12 (Bookworm) for native installation
- Docker (alternative, works on any architecture)

### Docker Installation (Recommended)

Works on any architecture (amd64, arm64, armv7).

```bash
cd server
docker compose build
docker compose up -d
```

The API will be available at `http://localhost:3123`

**Configuration UI:** `http://localhost:3123/config.html`

**With authentication:**
```bash
# Create .env file or pass directly
echo "API_TOKEN=your-secret-token" > .env
docker compose up -d
```

Then access: `http://localhost:3123/config.html?token=your-secret-token`

API calls require header: `Authorization: Bearer your-secret-token`

### Installation on Debian 12 (Clean Install)

1. **Update system and install basic tools:**
   ```bash
   sudo apt update && sudo apt upgrade -y
   sudo apt install -y curl git unzip gnupg
   ```

2. **Install Node.js (using NodeSource repository):**
   ```bash
   # Install Node.js 18.x (LTS)
   curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
   sudo apt install -y nodejs
   
   # Verify installation
   node -v
   npm -v
   ```

3. **Install System Dependencies for Puppeteer:**
   Puppeteer requires specific libraries to run Chrome/Chromium on Linux.
   ```bash
   sudo apt install -y ca-certificates fonts-liberation libasound2 \
   libatk-bridge2.0-0 libatk1.0-0 libc6 libcairo2 libcups2 libdbus-1-3 \
   libexpat1 libfontconfig1 libgbm1 libgcc1 libglib2.0-0 libgtk-3-0 \
   libnspr4 libnss3 libpango-1.0-0 libpangocairo-1.0-0 libstdc++6 \
   libx11-6 libx11-xcb1 libxcb1 libxcomposite1 libxcursor1 libxdamage1 \
   libxext6 libxfixes3 libxi6 libxrandr2 libxrender1 libxss1 libxtst6 \
   lsb-release wget xdg-utils
   ```

4. **Create Dedicated User:**
   Create a specific user for running the application to improve security.
   ```bash
   sudo adduser html2pngAppUser
   # Switch to the new user context
   su - html2pngAppUser
   ```

5. **Install Project:**
   *Perform this step as the `html2pngAppUser`.*
   ```bash
   git clone <repository-url> html2png-api
   cd html2png-api
   npm install
   ```
   (This command installs all Node.js dependencies, including Puppeteer.)

### Setup Systemd Service (Auto-start)

To ensure the API starts automatically on boot and restarts on failure, set up a `systemd` service.

1. **Create the service file:**
   ```bash
   sudo nano /etc/systemd/system/html2png.service
   ```

2. **Paste the following configuration:**
   ```ini
   [Unit]
   Description=HTML2PNG API Service
   After=network.target

   [Service]
   Type=simple
   User=html2pngAppUser
   WorkingDirectory=/home/html2pngAppUser/html2png-api
   ExecStart=/usr/bin/node server.js
   Restart=on-failure

   [Install]
   WantedBy=multi-user.target
   ```

3. **Enable and start the service:**
   ```bash
   # Reload systemd to recognize the new service
   sudo systemctl daemon-reload
   
   # Enable the service to start on boot
   sudo systemctl enable html2png
   
   # Start the service immediately
   sudo systemctl start html2png
   ```

4. **Manage the service:**
   ```bash
   # Check status
   sudo systemctl status html2png
   
   # Stop service
   sudo systemctl stop html2png
   
   # Restart service
   sudo systemctl restart html2png
   
   # View logs
   journalctl -u html2png -f
   ```

### Usage

1. **Start the server:**
   ```bash
   node server.js
   ```
   The server will start on port **3123**.

2. **Configuration UI:** `http://localhost:3123/config.html`
   
   Web interface to configure all rendering options with live preview.

3. **API Endpoint:** `POST /render`

   **Query Parameters:**
   - `url` (optional): The URL of the page to capture.
   - `mode` (optional): Special rendering modes.
     - `weather`: Renders the `index.html` file located in the server's `html` directory.
     - `demo`: Renders a test pattern for display calibration.
   - `format` (optional): Output image format: `png`, `bmp`, or `bwr` (3-color e-ink).
   - `width` (optional): Output image width in pixels. Defaults to `800`.
   - `height` (optional): Output image height in pixels. Defaults to `480`.
   - `colors` (optional): For `format=png`. Number of colors (2-256) for quantization.
   - `resizeAlgorithm` (optional): Interpolation method: `nearest`, `cubic`, `mitchell`, `lanczos2`, `lanczos3` (default).
   - `sharpen` (optional): Sharpening amount (0-2). Helps text clarity on e-ink.
   - `dither` (optional): `true` to enable Floyd-Steinberg dithering (works for BMP, BWR, PNG).

   **Body:**
   - Raw HTML string (Content-Type: `text/html`). Used only if `url` and `mode` are not provided.

   **Examples:**

   *Render a URL with default settings (PNG, 800x480):*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://example.com" --output example.png
   ```

   *Render a URL to BMP format with custom dimensions:*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://www.google.com&width=1280&height=720&format=bmp" --output google_search.bmp
   ```

   *Render HTML content (basic HTML to PNG with default dimensions):*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<h1>Simple HTML</h1><p>This is a test paragraph.</p>" "http://localhost:3123/render" --output simple.png
   ```

   *Render HTML content to PNG with color quantization:*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<html><body style='background: linear-gradient(to right, red, blue);'><div style='font-size: 50px; color: white;'>Gradient Test</div></body></html>" "http://localhost:3123/render?format=png&colors=32" --output gradient.png
   ```

   *Render HTML content to BMP format with custom dimensions:*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<div style='font-size: 30px; color: green;'>BMP Output</div>" "http://localhost:3123/render?width=600&height=300&format=bmp" --output custom_html.bmp
   ```

   *Render in weather mode (internal `html/index.html`) to PNG with custom dimensions:*
   ```bash
   curl -X POST "http://localhost:3123/render?mode=weather&width=400&height=240" --output weather_png.png
   ```

   *Render to BWR format for 3-color e-ink with sharpening and dithering:*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://example.com&format=bwr&sharpen=1&dither=true" --output display.bwr
   ```

   *Render using saved config (no parameters needed):*
   ```bash
   curl -X POST "http://localhost:3123/render" --output output.bmp
   ```

---

<a name="russian"></a>
## Русский

### Описание
API сервис на Node.js для конвертации HTML-контента или URL-адресов в изображения форматов PNG, BMP или BWR (3-цветный e-ink) с использованием Puppeteer. Поддерживает пользовательские размеры, квантование цвета, алгоритмы масштабирования, резкость, дизеринг Floyd-Steinberg для e-ink дисплеев и веб-интерфейс настройки.

### Требования
- Node.js (рекомендуется версия 18 или выше)
- npm
- Debian 12 (Bookworm) для нативной установки
- Docker (альтернатива, работает на любой архитектуре)

### Установка через Docker (Рекомендуется)

Работает на любой архитектуре (amd64, arm64, armv7).

```bash
cd server
docker compose build
docker compose up -d
```

API будет доступен по адресу `http://localhost:3123`

**Интерфейс настройки:** `http://localhost:3123/config.html`

**С аутентификацией:**
```bash
# Создайте .env файл или передайте напрямую
echo "API_TOKEN=ваш-секретный-токен" > .env
docker compose up -d
```

Доступ: `http://localhost:3123/config.html?token=ваш-секретный-токен`

API запросы требуют заголовок: `Authorization: Bearer ваш-секретный-токен`

### Установка на Debian 12 (с нуля)

1. **Обновление системы и установка базовых инструментов:**
   ```bash
   sudo apt update && sudo apt upgrade -y
   sudo apt install -y curl git unzip gnupg
   ```

2. **Установка Node.js (через репозиторий NodeSource):**
   ```bash
   # Установка Node.js 18.x (LTS)
   curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
   sudo apt install -y nodejs
   
   # Проверка установки
   node -v
   npm -v
   ```

3. **Установка системных зависимостей для Puppeteer:**
   Для работы Chrome/Chromium в Linux требуются специфические библиотеки.
   ```bash
   sudo apt install -y ca-certificates fonts-liberation libasound2 \
   libatk-bridge2.0-0 libatk1.0-0 libc6 libcairo2 libcups2 libdbus-1-3 \
   libexpat1 libfontconfig1 libgbm1 libgcc1 libglib2.0-0 libgtk-3-0 \
   libnspr4 libnss3 libpango-1.0-0 libpangocairo-1.0-0 libstdc++6 \
   libx11-6 libx11-xcb1 libxcb1 libxcomposite1 libxcursor1 libxdamage1 \
   libxext6 libxfixes3 libxi6 libxrandr2 libxrender1 libxss1 libxtst6 \
   lsb-release wget xdg-utils
   ```

4. **Создание выделенного пользователя:**
   Создайте отдельного пользователя для запуска приложения для повышения безопасности.
   ```bash
   sudo adduser html2pngAppUser
   # Переключитесь на нового пользователя
   su - html2pngAppUser
   ```

5. **Установка проекта:**
   *Выполняйте этот шаг от имени пользователя `html2pngAppUser`.*
   ```bash
   git clone <repository-url> html2png-api
   cd html2png-api
   npm install
   ```
   (Эта команда устанавливает все зависимости Node.js, включая Puppeteer.)

### Настройка автозапуска (Systemd)

Чтобы API запускался автоматически при старте системы и перезапускался при сбоях, настройте `systemd` сервис.

1. **Создайте файл сервиса:**
   ```bash
   sudo nano /etc/systemd/system/html2png.service
   ```

2. **Вставьте следующую конфигурацию:**
   ```ini
   [Unit]
   Description=HTML2PNG API Service
   After=network.target

   [Service]
   Type=simple
   User=html2pngAppUser
   WorkingDirectory=/home/html2pngAppUser/html2png-api
   ExecStart=/usr/bin/node server.js
   Restart=on-failure

   [Install]
   WantedBy=multi-user.target
   ```

3. **Включите и запустите сервис:**
   ```bash
   # Обновите конфигурацию systemd
   sudo systemctl daemon-reload
   
   # Добавьте сервис в автозагрузку
   sudo systemctl enable html2png
   
   # Запустите сервис
   sudo systemctl start html2png
   ```

4. **Управление сервисом:**
   ```bash
   # Проверка статуса
   sudo systemctl status html2png
   
   # Остановка сервиса
   sudo systemctl stop html2png
   
   # Перезапуск сервиса
   sudo systemctl restart html2png
   
   # Просмотр логов
   journalctl -u html2png -f
   ```

### Запуск и использование

1. **Запуск сервера:**
   ```bash
   node server.js
   ```
   Сервер будет запущен на порту **3123**.

2. **Интерфейс настройки:** `http://localhost:3123/config.html`
   
   Веб-интерфейс для настройки всех параметров рендеринга с предпросмотром.

3. **API Эндпоинт:** `POST /render`

   **Параметры запроса (Query Parameters):**
   - `url` (необязательно): URL страницы для захвата.
   - `mode` (необязательно): Специальные режимы рендеринга.
     - `weather`: Рендерит файл `index.html` из директории `html` сервера.
     - `demo`: Рендерит тестовый паттерн для калибровки дисплея.
   - `format` (необязательно): Формат изображения: `png`, `bmp` или `bwr` (3-цветный e-ink).
   - `width` (необязательно): Ширина изображения в пикселях. По умолчанию `800`.
   - `height` (необязательно): Высота изображения в пикселях. По умолчанию `480`.
   - `colors` (необязательно): Для `format=png`. Количество цветов (2-256) для квантования.
   - `resizeAlgorithm` (необязательно): Метод интерполяции: `nearest`, `cubic`, `mitchell`, `lanczos2`, `lanczos3` (по умолчанию).
   - `sharpen` (необязательно): Уровень резкости (0-2). Улучшает читаемость текста на e-ink.
   - `dither` (необязательно): `true` для включения дизеринга Floyd-Steinberg (работает для BMP, BWR, PNG).

   **Тело запроса (Body):**
   - Строка HTML (Content-Type: `text/html`). Используется только если `url` и `mode` не указаны.

   **Примеры:**

   *Рендер URL с настройками по умолчанию (PNG, 800x480):*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://example.com" --output example.png
   ```

   *Рендер URL в формат BMP с пользовательскими размерами:*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://www.google.com&width=1280&height=720&format=bmp" --output google_search.bmp
   ```

   *Рендер HTML контента (базовый HTML в PNG с размерами по умолчанию):*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<h1>Простой HTML</h1><p>Это тестовый параграф.</p>" "http://localhost:3123/render" --output simple.png
   ```

   *Рендер HTML контента в PNG с квантованием цвета:*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<html><body style='background: linear-gradient(to right, red, blue);'><div style='font-size: 50px; color: white;'>Тест градиента</div></body></html>" "http://localhost:3123/render?format=png&colors=32" --output gradient.png
   ```

   *Рендер HTML контента в формат BMP с пользовательскими размерами:*
   ```bash
   curl -X POST -H "Content-Type: text/html" -d "<div style='font-size: 30px; color: green;'>Вывод BMP</div>" "http://localhost:3123/render?width=600&height=300&format=bmp" --output custom_html.bmp
   ```

   *Рендер в режиме weather (внутренний `html/index.html`) в PNG с пользовательскими размерами:*
   ```bash
   curl -X POST "http://localhost:3123/render?mode=weather&width=400&height=240" --output weather_png.png
   ```

   *Рендер в BWR формат для 3-цветного e-ink с резкостью и дизерингом:*
   ```bash
   curl -X POST "http://localhost:3123/render?url=https://example.com&format=bwr&sharpen=1&dither=true" --output display.bwr
   ```

   *Рендер с использованием сохранённой конфигурации (параметры не нужны):*
   ```bash
   curl -X POST "http://localhost:3123/render" --output output.bmp
   ```