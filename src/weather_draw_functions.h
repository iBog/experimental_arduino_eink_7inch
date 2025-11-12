#ifndef WEATHER_DRAW_FUNCTIONS_H_
#define WEATHER_DRAW_FUNCTIONS_H_

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <GxEPD2_3C.h>

// Forward declaration of display object
extern GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display;

// Color definitions
#define FG_COLOR GxEPD_BLACK
#define BG_COLOR GxEPD_WHITE

// Icon size definitions
#define LargeIcon true
#define SmallIcon false
#define Large 10
#define Small 3

typedef struct {
    int x;
    int y;
    int scale;
    int lineSize;
    bool iconSize;
    bool night;
} DrawContext;

// Function declarations
void draw_wx_icon(int x, int y, const String& iconCode, bool iconSize);
void sunny(DrawContext ctx);
void few_clouds(DrawContext ctx);
void clouds(DrawContext ctx);
void heavy_clouds(DrawContext ctx);
void shower_rain(DrawContext ctx);
void rain(DrawContext ctx);
void thunder_storm(DrawContext ctx);
void snow(DrawContext ctx);
void haze(DrawContext ctx);
void no_data(DrawContext ctx);

void addcloud(DrawContext ctx, bool white);
void addsun(DrawContext ctx);
void addmoon(DrawContext ctx);
void addrain(DrawContext ctx);
void addsnow(DrawContext ctx);
void addtstorm(DrawContext ctx);
void addfog(DrawContext ctx);

DrawContext offsetXYS(DrawContext ctx, int x, int y, float scale);
DrawContext offsetXY(DrawContext ctx, int x, int y);
DrawContext offsetMoon(DrawContext ctx);

// Temperature drawing function
typedef struct {
    int x;
    int y;
    int h;
    int w;
} Bounds;

Bounds draw_temp(int x, int y, int size, float temp, const GFXfont* font);
Bounds draw_string(int x, int y, String text, int alignment);

// Implementation

void draw_wx_icon(int x, int y, const String& iconCode, bool iconSize)
{
    DrawContext ctx;
    ctx.x = x;
    ctx.y = y;
    ctx.iconSize = iconSize;
    ctx.night = iconCode.endsWith("n");
    ctx.lineSize = iconSize ? 2 : 1;
    ctx.scale = iconSize ? Large : Small;

    String icon = iconCode.substring(0, 2);

    if (icon == "01")
        sunny(ctx);
    else if (icon == "02")
        few_clouds(ctx);
    else if (icon == "03")
        clouds(ctx);
    else if (icon == "04")
        heavy_clouds(ctx);
    else if (icon == "09")
        shower_rain(ctx);
    else if (icon == "10")
        rain(ctx);
    else if (icon == "11")
        thunder_storm(ctx);
    else if (icon == "13")
        snow(ctx);
    else if (icon == "50")
        haze(ctx);
    else
        no_data(ctx);
}

void addcloud(DrawContext ctx, bool white)
{
    const int x = ctx.x;
    const int y = ctx.y;
    const int scale = ctx.scale;
    const int linesize = ctx.lineSize;

    // Draw cloud outer
    display.fillCircle(x - scale * 3, y, scale, FG_COLOR);
    display.fillCircle(x + scale * 3, y, scale, FG_COLOR);
    display.fillCircle(x - scale, y - scale, scale * 1.4, FG_COLOR);
    display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, FG_COLOR);
    display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, FG_COLOR);

    if (white) {
        // Clear cloud inner
        display.fillCircle(x - scale * 3, y, scale - linesize, BG_COLOR);
        display.fillCircle(x + scale * 3, y, scale - linesize, BG_COLOR);
        display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, BG_COLOR);
        display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, BG_COLOR);
        display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, BG_COLOR);
    }
}

void addsun(DrawContext ctx)
{
    int x = ctx.x;
    int y = ctx.y;
    int scale = ctx.scale;
    int linesize = ctx.lineSize;

    display.drawLine(x - scale * 1.8, y, x + scale * 1.8, y, FG_COLOR);
    display.drawLine(x, y + scale * 1.8, x, y - scale * 1.8, FG_COLOR);
    display.drawLine(x - scale * 1.35, y - scale * 1.35, x + scale * 1.35, y + scale * 1.35, FG_COLOR);
    display.drawLine(x - scale * 1.35, y + scale * 1.35, x + scale * 1.35, y - scale * 1.35, FG_COLOR);

    if (ctx.iconSize == LargeIcon) {
        display.drawLine(x - scale * 1.8 + 1, y - 1, x + scale * 1.8 - 1, y - 1, FG_COLOR);
        display.drawLine(x - scale * 1.8 + 1, y + 1, x + scale * 1.8 - 1, y + 1, FG_COLOR);
        display.drawLine(x - 1, y + scale * 1.8 - 1, x - 1, y - scale * 1.8 + 1, FG_COLOR);
        display.drawLine(x + 1, y + scale * 1.8 - 1, x + 1, y - scale * 1.8 + 1, FG_COLOR);
        display.drawLine(x - scale * 1.35, y - scale * 1.35 + 1, x + scale * 1.35 - 1, y + scale * 1.35, FG_COLOR);
        display.drawLine(x - scale * 1.35 + 1, y - scale * 1.35, x + scale * 1.35, y + scale * 1.35 - 1, FG_COLOR);
        display.drawLine(x - scale * 1.35 + 1, y + scale * 1.35, x + scale * 1.35, y - scale * 1.35 + 1, FG_COLOR);
        display.drawLine(x - scale * 1.35, y + scale * 1.35 - 1, x + scale * 1.35 - 1, y - scale * 1.35, FG_COLOR);
    }

    if (scale > Large) {
        display.fillCircle(x, y, scale + linesize, BG_COLOR);
    }
    display.fillCircle(x, y, scale, FG_COLOR);
    display.fillCircle(x, y, scale - linesize, BG_COLOR);
}

void addmoon(DrawContext ctx)
{
    display.fillCircle(ctx.x, ctx.y, ctx.scale, FG_COLOR);
    display.fillCircle(ctx.x + ctx.scale * 0.9, ctx.y, ctx.scale, BG_COLOR);
}

void addrain(DrawContext ctx)
{
    int x = ctx.x;
    int y = ctx.y - ctx.lineSize;
    int scale = ctx.scale;

    // Simple rain lines
    for (int i = 0; i < 3; i++) {
        int rx = x - scale + i * scale;
        display.drawLine(rx, y + scale * 2, rx - scale / 2, y + scale * 4, FG_COLOR);
    }
}

void addsnow(DrawContext ctx)
{
    int x = ctx.x;
    int y = ctx.y;
    int scale = ctx.scale;

    // Simple snow flakes as *
    // Font references removed to avoid conflicts
    display.setTextColor(FG_COLOR);
    display.setCursor(x - scale, y + scale * 3);
    display.print("* * *");
}

void addtstorm(DrawContext ctx)
{
    int x = ctx.x;
    int y = ctx.y;
    int scale = ctx.scale;

    // Simple lightning bolt
    display.drawLine(x, y, x - scale, y + scale * 2, FG_COLOR);
    display.drawLine(x - scale, y + scale * 2, x + scale / 2, y + scale, FG_COLOR);
    display.drawLine(x + scale / 2, y + scale, x - scale / 2, y + scale * 3, FG_COLOR);
}

void addfog(DrawContext ctx)
{
    for (int i = 0; i < 3; i++) {
        display.fillRect(ctx.x - ctx.scale * 2, ctx.y - ctx.scale + i * ctx.scale, ctx.scale * 4, ctx.lineSize, FG_COLOR);
    }
}

DrawContext offsetXYS(DrawContext ctx, int x, int y, float scale)
{
    DrawContext off = ctx;
    off.x = off.x + x;
    off.y = off.y + y;
    off.scale = off.scale * scale;
    return off;
}

DrawContext offsetXY(DrawContext ctx, int x, int y)
{
    DrawContext off = ctx;
    off.x = off.x + x;
    off.y = off.y + y;
    return off;
}

DrawContext offsetMoon(DrawContext ctx)
{
    return offsetXY(ctx, -ctx.scale * 3.6, -ctx.scale * 2.8);
}

DrawContext offsetX(DrawContext ctx, int x)
{
    DrawContext off = ctx;
    off.x = off.x + x;
    return off;
}

DrawContext offsetY(DrawContext ctx, int y)
{
    DrawContext off = ctx;
    off.y = off.y + y;
    return off;
}

void sunny(DrawContext ctx)
{
    if (ctx.night)
        addmoon(offsetXYS(ctx, -0.7 * ctx.scale, -0.7 * ctx.scale, 2.34));
    else
        addsun(offsetXYS(ctx, -0.7 * ctx.scale, -0.7 * ctx.scale, 2.34));
}

void few_clouds(DrawContext ctx)
{
    addcloud(offsetXYS(ctx, 0, 0, 0.9), true);
    if (ctx.night) {
        addmoon(offsetMoon(ctx));
    } else {
        addsun(offsetMoon(ctx));
    }
}

void clouds(DrawContext ctx)
{
    if (ctx.night) {
        addmoon(offsetMoon(ctx));
    } else {
        addsun(offsetMoon(ctx));
    }
    addcloud(ctx, true);
}

void heavy_clouds(DrawContext ctx)
{
    addcloud(offsetXYS(ctx, -4, -5, 0.9), false);
    addcloud(offsetX(ctx, 3), true);
}

void rain(DrawContext ctx)
{
    if (ctx.night)
        addmoon(offsetMoon(ctx));
    addcloud(ctx, true);
    addrain(ctx);
}

void shower_rain(DrawContext ctx)
{
    heavy_clouds(ctx);
    addrain(offsetX(ctx, 3));
}

void thunder_storm(DrawContext ctx)
{
    heavy_clouds(ctx);
    addtstorm(ctx);
}

void snow(DrawContext ctx)
{
    if (ctx.night)
        addmoon(offsetMoon(ctx));
    addcloud(ctx, true);
    addsnow(ctx);
}

void haze(DrawContext ctx)
{
    if (ctx.night)
        addmoon(offsetMoon(ctx));
    else
        addsun(offsetMoon(ctx));
    addfog(offsetY(ctx, 3));
}

void no_data(DrawContext ctx)
{
    // Font references removed to avoid conflicts
    display.setTextColor(FG_COLOR);
    display.setCursor(ctx.x - 10, ctx.y + 5);
    display.print("?");
}

Bounds draw_string(int x, int y, String text, int alignment)
{
    int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
    uint16_t w, h;
    display.setTextWrap(false);
    display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
    if (alignment == 2) // RIGHT
    {
        x = x - w;
        x1 = x1 - w;
    }
    if (alignment == 1) // CENTER
    {
        x = x - w / 2;
        x1 = x1 - w / 2;
    }
    display.setCursor(x, y);
    Bounds b;
    b.x = x1;
    b.y = y1;
    b.w = w;
    b.h = h;
    display.print(text);
    return b;
}

Bounds draw_temp(int x, int y, int size, float temp, const GFXfont* font)
{
    display.setFont(font);
    Bounds bounds = draw_string(x, y, String(temp, 0), 0); // LEFT alignment
    display.drawCircle(bounds.x + 1.2 * bounds.w, bounds.y + size, size, GxEPD_RED);
    bounds.w = 1.2 * bounds.w + size;
    return bounds;
}

#endif
