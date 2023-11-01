#define AUDIO_FILENAME "/44100_u16le.pcm"
#define FPS 30
#define MJPEG_FILENAME "/220_30fps.mjpeg"
#define MJPEG_BUFFER_SIZE (220 * 176 * 2 / 4)
//#define FPS 15
//#define MJPEG_FILENAME "/320_15fps.mjpeg"
//#define MJPEG_BUFFER_SIZE (320 * 240 * 2 / 4)
#define READ_BUFFER_SIZE 2048
/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       12
 *    D3       13
 *    CMD      15
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      14
 *    VSS      GND
 *    D0       2  (add 1K pull up after flashing)
 *    D1       4
 */

#include <WiFi.h>
#include <SD_MMC.h>

#include <Arduino_GFX_Library.h>
#define TFT_BRIGHTNESS 128
 // ST7789 Display
 // #define TFT_BL 32
 // Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
 // Arduino_GFX *gfx = new Arduino_ST7789(bus, 33 /* RST */, 3 /* rotation */, true /* IPS */);
 // ILI9225 Display
#define TFT_BL 22
Arduino_ESP32SPI* bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
Arduino_GFX* gfx = new Arduino_ILI9225(bus, 33 /* RST */, 3 /* rotation */);
// ILI9341 Display
// #define TFT_BL 22
// Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
// Arduino_GFX *gfx = new Arduino_ILI9341(bus, 33 /* RST */, 3 /* rotation */);

#include "MjpegClass.h"
static MjpegClass mjpeg;

int next_frame = 0;
int skipped_frames = 0;
unsigned long total_read_audio = 0;
unsigned long total_play_audio = 0;
unsigned long total_read_video = 0;
unsigned long total_play_video = 0;
unsigned long total_remain = 0;
unsigned long start_ms, curr_ms, next_frame_ms;
void setup()
{
    WiFi.mode(WIFI_OFF);
    Serial.begin(115200);

    // Init Video
    gfx->begin();
    gfx->fillScreen(BLACK);

#ifdef TFT_BL
    ledcAttachPin(TFT_BL, 1);     // assign TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8);       // 12 kHz PWM, 8-bit resolution
    ledcWrite(1, TFT_BRIGHTNESS); // brightness 0 - 255
#endif

    // Init SD card
    // if ((!SD_MMC.begin()) && (!SD_MMC.begin())) /* 4-bit SD bus mode */
    if ((!SD_MMC.begin("/sdcard", true)) && (!SD_MMC.begin("/sdcard", true))) /* 1-bit SD bus mode */
    {
        Serial.println(F("ERROR: SD card mount failed!"));
        gfx->println(F("ERROR: SD card mount failed!"));
    }
    else
    {
        {
            
            {
                File vFile = SD_MMC.open(MJPEG_FILENAME);
                if (!vFile || vFile.isDirectory())
                {
                    Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
                    gfx->println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
                }
                else
                {
                    {
                        uint8_t* mjpeg_buf = (uint8_t*)malloc(MJPEG_BUFFER_SIZE);
                        if (!mjpeg_buf)
                        {
                            Serial.println(F("mjpeg_buf malloc failed!"));
                        }
                        else
                        {
                            Serial.println(F("PCM audio MJPEG video start"));
                            start_ms = millis();
                            curr_ms = millis();
                            mjpeg.setup(vFile, mjpeg_buf, (Arduino_TFT*)gfx, true);
                            next_frame_ms = start_ms + (++next_frame * 1000 / FPS);

                            while (vFile.available())
                            {
                                total_read_audio += millis() - curr_ms;
                                curr_ms = millis();

                                total_play_audio += millis() - curr_ms;
                                curr_ms = millis();

                                // Read video
                                mjpeg.readMjpegBuf();
                                total_read_video += millis() - curr_ms;
                                curr_ms = millis();

                                if (millis() < next_frame_ms) // check show frame or skip frame
                                {
                                    // Play video
                                    mjpeg.drawJpg();
                                    total_play_video += millis() - curr_ms;

                                    int remain_ms = next_frame_ms - millis();
                                    total_remain += remain_ms;
                                    if (remain_ms > 0)
                                    {
                                        delay(remain_ms);
                                    }
                                }
                                else
                                {
                                    ++skipped_frames;
                                    Serial.println(F("Skip frame"));
                                }

                                curr_ms = millis();
                                next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
                            }
                            int time_used = millis() - start_ms;
                            Serial.println(F("PCM audio MJPEG video end"));
                            vFile.close();
                            int played_frames = next_frame - 1 - skipped_frames;
                            float fps = 1000.0 * played_frames / time_used;
                            Serial.printf("Played frames: %d\n", played_frames);
                            Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
                            Serial.printf("Time used: %d ms\n", time_used);
                            Serial.printf("Expected FPS: %d\n", FPS);
                            Serial.printf("Actual FPS: %0.1f\n", fps);
                            Serial.printf("SDMMC read PCM: %d ms (%0.1f %%)\n", total_read_audio, 100.0 * total_read_audio / time_used);
                            Serial.printf("Play audio: %d ms (%0.1f %%)\n", total_play_audio, 100.0 * total_play_audio / time_used);
                            Serial.printf("SDMMC read MJPEG: %d ms (%0.1f %%)\n", total_read_video, 100.0 * total_read_video / time_used);
                            Serial.printf("Play video: %d ms (%0.1f %%)\n", total_play_video, 100.0 * total_play_video / time_used);
                            Serial.printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);

#define CHART_MARGIN 24
#define LEGEND_A_COLOR 0xE0C3
#define LEGEND_B_COLOR 0x33F7
#define LEGEND_C_COLOR 0x4D69
#define LEGEND_D_COLOR 0x9A74
#define LEGEND_E_COLOR 0xFBE0
#define LEGEND_F_COLOR 0xFFE6
#define LEGEND_G_COLOR 0xA2A5
                            gfx->setCursor(0, 0);
                            gfx->setTextColor(WHITE);
                            gfx->printf("Played frames: %d\n", played_frames);
                            gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
                            gfx->printf("Actual FPS: %0.1f\n\n", fps);
                            int16_t r1 = ((gfx->height() - CHART_MARGIN - CHART_MARGIN) / 2);
                            int16_t r2 = r1 / 2;
                            int16_t cx = gfx->width() - gfx->height() + CHART_MARGIN + CHART_MARGIN - 1 + r1;
                            int16_t cy = r1 + CHART_MARGIN;
                            float arc_start = 0;
                            float arc_end = max(2.0, 360.0 * total_read_audio / time_used);
                            for (int i = arc_start + 1; i < arc_end; i += 2)
                            {
                                gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_D_COLOR);
                            }
                            gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_D_COLOR);
                            gfx->setTextColor(LEGEND_D_COLOR);
                            gfx->printf("Read PCM:\n%0.1f %%\n", 100.0 * total_read_audio / time_used);

                            arc_start = arc_end;
                            arc_end += max(2.0, 360.0 * total_play_audio / time_used);
                            for (int i = arc_start + 1; i < arc_end; i += 2)
                            {
                                gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_C_COLOR);
                            }
                            gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_C_COLOR);
                            gfx->setTextColor(LEGEND_C_COLOR);
                            gfx->printf("Play audio:\n%0.1f %%\n", 100.0 * total_play_audio / time_used);

                            arc_start = arc_end;
                            arc_end += max(2.0, 360.0 * total_read_video / time_used);
                            for (int i = arc_start + 1; i < arc_end; i += 2)
                            {
                                gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, i - 90.0, LEGEND_B_COLOR);
                            }
                            gfx->fillArc(cx, cy, r1, r2, arc_start - 90.0, arc_end - 90.0, LEGEND_B_COLOR);
                            gfx->setTextColor(LEGEND_B_COLOR);
                            gfx->printf("Read MJPEG:\n%0.1f %%\n", 100.0 * total_read_video / time_used);

                            arc_start = arc_end;
                            arc_end += max(2.0, 360.0 * total_play_video / time_used);
                            for (int i = arc_start + 1; i < arc_end; i += 2)
                            {
                                gfx->fillArc(cx, cy, r1, 0, arc_start - 90.0, i - 90.0, LEGEND_A_COLOR);
                            }
                            gfx->fillArc(cx, cy, r1, 0, arc_start - 90.0, arc_end - 90.0, LEGEND_A_COLOR);
                            gfx->setTextColor(LEGEND_A_COLOR);
                            gfx->printf("Play video:\n%0.1f %%\n", 100.0 * total_play_video / time_used);

                            
                            // avoid unexpected output at audio pins
                            pinMode(25, OUTPUT);
                            digitalWrite(25, LOW);
                            pinMode(26, OUTPUT);
                            digitalWrite(26, LOW);
                        }
                    }
                }
            }
        }
    }
#ifdef TFT_BL
    delay(60000);
    ledcDetachPin(TFT_BL);
#endif
    // gfx->displayOff();
    // esp_deep_sleep_start();
}

void loop()
{
}
