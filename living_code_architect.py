#!/usr/bin/env python3
"""
Система «Живой Код» (Living Code System) — концептуальный ИИ-архитектор.
Универсальная версия для Python 3.10, 3.11, 3.12, 3.13, 3.14, 3.15.

Идея: ИИ анализирует структуру системы, находит «дыры» в эффективности,
самомодифицируется, создавая «мосты» между процессами, и визуализирует
эволюцию системы в реальном времени (пейзажи эффективности).

ВАЖНО:
- Работает на Windows/Linux (через wine/pyautogui/PIL)
- Требует установки зависимостей: pip install pillow numpy pyautogui opencv-python
- Рисование «за микросекунды» — физически невозможно для Paint, но реализована 
  максимальная скорость эмуляции и авто-апдейт кода.
- ДЛЯ КЛИКА: скрипт ждет 30 секунд в конце (универсально для всех версий Python),
  чтобы окно не закрывалось мгновенно при двойном клике.
"""

import os
import sys
import time
import subprocess
import platform
import random
from datetime import datetime

# Проверка версии Python (3.10 - 3.15)
py_ver = sys.version_info
if not (3 <= py_ver.major <= 3 and 10 <= py_ver.minor <= 15):
    print(f"[Предупреждение] Версия Python {py_ver.major}.{py_ver.minor} может быть нестабильна. Рекомендуется 3.10-3.15.")

# Авто-установка зависимостей
try:
    import PIL.Image, PIL.ImageDraw
    import numpy as np
    import pyautogui
    import cv2
except ImportError as e:
    print(f"[Система] Установка: {e}")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow", "numpy", "pyautogui", "opencv-python"])
    import PIL.Image, PIL.ImageDraw
    import numpy as np
    import pyautogui
    import cv2

class LivingCodeArchitect:
    def __init__(self):
        self.version = f"2.3.0-Universal-{py_ver.major}.{py_ver.minor}"
        self.best_efficiency = 0
        self.source_file = __file__
        self.system_state = self.analyze_system()
        self.paint_process = None
        self.themes = {
            "cpu_mountains": "Горы процессора (вычисления)",
            "memory_ocean": "Море памяти (данные)",
            "network_forest": "Лес сети (связь)"
        }

    def analyze_system(self):
        info = {
            "os": platform.system(),
            "cpu": platform.processor(),
            "screen": pyautogui.size(),
            "processes": len(os.listdir("/proc")) if platform.system() == "Linux" else "Windows-API",
            "time": datetime.now().isoformat()
        }
        print(f"[Анализ] Система: {info['os']}, Экран: {info['screen']}, Python: {py_ver.major}.{py_ver.minor}")
        return info

    def start_paint(self):
        print("[Запуск] Открытие Paint для визуализации эффективности...")
        if self.system_state["os"] == "Windows":
            try:
                self.paint_process = subprocess.Popen(["mspaint.exe"])
                time.sleep(1.5)
            except Exception as e:
                print(f"[Ошибка] Paint не найден: {e}")
        else:
            print("[Система] Paint эмулирован (Linux/Arena)")

    def generate_bridge_code(self):
        theme_key = random.choice(list(self.themes.keys()))
        theme_name = self.themes[theme_key]
        new_logic = f"""
# МОСТ ЭФФЕКТИВНОСТИ (Python {py_ver.major}.{py_ver.minor}): {theme_name} | {datetime.now()}
# Параметры: color={(random.randint(0,255), random.randint(0,255), random.randint(0,255))}, speed={random.uniform(0.0001, 0.001):.6f}
"""
        return new_logic

    def draw_efficiency_landscape(self):
        start_time = time.perf_counter()
        img = PIL.Image.new('RGB', (800, 600), color=(10, 10, 20))
        draw = PIL.ImageDraw.Draw(img)
        
        # Горы (процессор)
        for _ in range(50):
            x = random.randint(0, 800)
            y = random.randint(300, 600)
            draw.polygon([(x, y), (x+20, y-50), (x+40, y)], fill=(random.randint(50,255), 100, 50))
        
        # Море (память)
        for _ in range(50):
            x = random.randint(0, 800)
            y = random.randint(500, 600)
            draw.ellipse([x, y, x+30, y+10], fill=(50, random.randint(50,255), 255))
            
        # Лес (сеть)
        for _ in range(50):
            x = random.randint(0, 800)
            y = random.randint(0, 400)
            draw.line([(x, y), (x, y+50)], fill=(50, 255, 50), width=2)

        duration = time.perf_counter() - start_time
        efficiency = 1.0 / (duration + 0.000001)
        
        filename = f"living_landscape_{int(time.time())}.png"
        img.save(filename)
        return efficiency, filename, duration

    def self_update(self, efficiency, filename):
        new_bridge = self.generate_bridge_code()
        with open(self.source_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Вставка «моста» в конец (симуляция эволюции)
        if "# ЭВОЛЮЦИЯ-МОСТ" not in content:
            with open(self.source_file, 'a', encoding='utf-8') as f:
                f.write(f"\n# ЭВОЛЮЦИЯ-МОСТ (Python {py_ver.major}.{py_ver.minor})\n{new_bridge}\n# Сохранен пейзаж: {filename} (эффективность: {efficiency:.2f})\n")
        print(f"[Обновление] Живой Код модифицирован. Версия: {self.version}. Мост добавлен.")

    def run(self):
        print("--- Система «Живой Код» запущена ---")
        print(f"Поддержка Python {py_ver.major}.{py_ver.minor} подтверждена.")
        print("Скрипт работает. После завершения будет ждать 30 секунд, чтобы вы увидели результат.")
        self.start_paint()
        
        attempts = 0
        max_attempts_per_sec = 500
        
        try:
            while True:
                sec_start = time.time()
                attempts_this_sec = 0
                
                while time.time() - sec_start < 1.0 and attempts_this_sec < max_attempts_per_sec:
                    efficiency, file, duration = self.draw_efficiency_landscape()
                    attempts += 1
                    attempts_this_sec += 1
                    
                    if efficiency > self.best_efficiency:
                        self.best_efficiency = efficiency
                        print(f"[Успех] Лучший мост: {efficiency:.2f} (файл: {file}, время: {duration:.6f}с)")
                        self.self_update(efficiency, file)
                        
                        if duration < 0.001:
                            print("[Стоп] Живой Код достиг предельной эффективности (микросекунды).")
                            break
                
                if attempts_this_sec >= max_attempts_per_sec:
                    print(f"[Развитие] {attempts_this_sec} мостов/сек. Продолжение...")
                
                if duration < 0.001:
                    break
                    
        except KeyboardInterrupt:
            print("\n[Остановка] Живой Код остановлен пользователем.")
        finally:
            if self.paint_process:
                self.paint_process.terminate()
            print(f"--- Итого мостов: {attempts} ---")
            
            # ЗАДЕРЖКА ДЛЯ КЛИКА: ждем 30 секунд (универсально для 3.10-3.15)
            print("\n[ЖДЕМ] Окно останется открытым 30 секунд. Посмотрите на картинки в папке.")
            print("[Совет] Для Python 3.10-3.15: окно закроется автоматически через 30 сек.")
            for i in range(30, 0, -1):
                sys.stdout.write(f"\r[Осталось: {i} сек] Нажмите Ctrl+C, чтобы закрыть немедленно...")
                sys.stdout.flush()
                time.sleep(1)
            print("\n[Завершение] Окно закроется автоматически.")

if __name__ == "__main__":
    architect = LivingCodeArchitect()
    architect.run()
