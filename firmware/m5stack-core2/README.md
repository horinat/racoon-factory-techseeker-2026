# M5Stack Core2 Firmware

TechSeeker 2026 / Team U "RacoonFactory" 用のM5Stack Core2ファームウェアです。

## Features

- スタート入力
- リスタート/再開入力
- 的検出入力
- ヒット時のサーボPWM出力
- 60秒ゲーム終了後にスコアをHTTPS APIへ送信

## Pin Assignment

| 機能 | GPIO | Core2ポート | pinMode | ON判定 |
|---|---:|---|---|---|
| スタート | GPIO32 | PORT-A 黄 | INPUT_PULLUP | LOW |
| リスタート/再開 | GPIO14 | PORT-C 黄 | INPUT_PULLUP | LOW |
| 的検出 | GPIO26 | PORT-B 黄 | INPUT_PULLUP | LOW |
| サーボ信号 | GPIO13 | PORT-C 白 | PWM出力 | - |

GPIO36は入力専用で内部プルアップが使えないため、的検出はGPIO26(PORT-B 黄)へ変更しています。

サーボはCore2からPWM信号だけを出します。サーボ本体の電源は外部5Vを使い、GNDはCore2と共通にしてください。

## Setup

PlatformIOで開いて、`src/main.cpp`冒頭の値を設定します。

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_KEY = "YOUR_API_KEY";
```

Wi-FiパスワードやAPIキーはGitHubへコミットしないでください。公開用コードではプレースホルダーのままにします。

## Servo Tuning

サーボ動作角度は以下で調整します。

```cpp
constexpr int SERVO_IDLE_ANGLE = 0;
constexpr int SERVO_SEQUENCE_ANGLES[] = {120, 140, 90, 0};
constexpr uint32_t SERVO_SEQUENCE_INTERVALS_MS[] = {100, 100, 100, 100};
```
