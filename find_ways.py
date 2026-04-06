#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "folium",
#     "geopy",
#     "networkx",
#     "osmnx",
# ]
# ///
import folium
import logging
import argparse
import subprocess
import csv
import os
import sys

from pathlib import Path
from geopy.geocoders import Nominatim

logging.basicConfig(
    level=logging.INFO,
    format="%(levelname)s: %(message)s"
)

logger = logging.getLogger(__name__)

# простите мне глобалы?👉🏻👈🏻
GEOCODER = Nominatim(user_agent="spb_route_finder")
CITY_QUERY = ", Санкт-Петербург, Россия"
INPUT_FILE = "task/input.txt"
OUTPUT_FILE = "task/output.txt"
C_EXECUTABLE = "./path_processor"


def geocode_address(address: str) -> tuple[float, float]:
    """
    Преобразует адрес в координаты (широта, долгота).
    Гарантированно ищет в Санкт-Петербурге.
    """

    try:
        location = GEOCODER.geocode(address + CITY_QUERY, timeout=10)
    except Exception as e:
        logger.error("Ошибка геолокации '%s': %s", address, e)
        raise 

    if not location:
        raise Exception('Ашибачка! Не получилось закодировать адрес')
    
    return location.latitude, location.longitude


def save_coords_to_txt(lat1: float, lon1: float, lat2: float, lon2: float, filepath: str | Path) -> None:
    """Сохраняет координаты в TXT файл (формат: широта долгота, по одной паре на строку)."""

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(f"{lat1} {lon1}\n")
        f.write(f"{lat2} {lon2}\n")

    logger.info("Координаты сохранены в %s", filepath)


def run_c_processor(input_path: str, output_path: str) -> bool:
    """
    Запускает скомпилированный C-файл.
    Передаёт пути к входному и выходному файлам как аргументы.
    """
    # gcc graph.c ../../lab3/list/generic.c ../../lab3/vector/generic.c ../../lab4/hash_table/generic.c -o path_processor

    if not os.path.isfile(C_EXECUTABLE):
        logger.error("Не найден исполняемый файл: %s", C_EXECUTABLE)
        logger.error("Скомпилируйте: gcc path_processor.c -o %s -lm", C_EXECUTABLE)

        return False
    
    try:
        result = subprocess.run(
            [C_EXECUTABLE, "data", input_path, output_path],
            capture_output=True,
            text=True,
            check=True,
            timeout=30
        )
    except subprocess.CalledProcessError as e:
        logger.error("Ошибка выполнения C: %s", e)

        if e.stderr:
            logger.error("stderr: %s", e.stderr)

        return False
    except Exception as e:
        logger.error("Ошибка запуска: %s", e)

        return False
    
    if result.stdout:
        logger.info("C-вывод: %s", result.stdout.strip())
    
    return True


def read_path_from_csv(filepath: str) -> list[tuple[float, float]]:
      """Читает путь из CSV файла: каждая строка — lat lon."""

      with open(filepath, encoding='utf-8') as f:
          return [
                  (float(r[0]), float(r[1])) 
                  for r in csv.reader(f, delimiter=' ') 
                  if len(r) >= 2
            ]


def draw_map(path: list[tuple[float, float]], output_html: str = "route_map.html"):
    """
    Отрисовывает интерактивную карту с маршрутом через Folium.
    Сохраняет результат в HTML-файл.
    """

    if not path:
        logger.info("Пустой путь, нечего рисовать")
        return
    
    center = (
        sum(p[0] for p in path) / len(path),
        sum(p[1] for p in path) / len(path)
    )
    
    m = folium.Map(location=center, zoom_start=13, tiles='OpenStreetMap')
    
    for i, (lat, lon) in enumerate(path):
        color = 'green' if i == 0 else ('red' if i == len(path) - 1 else 'blue')
        label = 'Старт' if i == 0 else ('Финиш' if i == len(path) - 1 else f'Точка {i+1}')
        
        folium.Marker(
            location=[lat, lon],
            popup=f"{label}: {lat:.5f}, {lon:.5f}",
            icon=folium.Icon(color=color, icon='info-sign')
        ).add_to(m)
    
    folium.PolyLine(
        locations=path,
        color='blue',
        weight=4,
        opacity=0.8,
        tooltip="Маршрут"
    ).add_to(m)
    
    m.save(output_html)
    logger.info("Карта сохранена: %s", output_html)
    logger.info("Откройте файл в браузере для просмотра")


def main():
    parser = argparse.ArgumentParser(
        prog="./find_ways",
        usage="%(prog)s [--interactive]",
        epilog="Программа для нахождения маршрута между двумя точками"
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="интерактивный ввод адресов"
    )
    args = parser.parse_args()
    
    logger.info("Маршрутизатор Санкт-Петербурга")
    logger.info("=" * 50)

    if args.interactive:

        logger.info("\nВведите адреса (гарантированно в СПб):")

        addr1 = input("   Адрес отправления: ").strip()
        addr2 = input("   Адрес назначения:  ").strip()

        if not addr1 or not addr2:
            logger.info("Адреса не могут быть пустыми")
            sys.exit(1)

        logger.info("\nПоиск координат...")
        coords1 = geocode_address(addr1)
        coords2 = geocode_address(addr2)

        lat1, lon1 = coords1
        lat2, lon2 = coords2

        logger.info("%s → (%.6f, %.6f)", addr1, lat1, lon1)
        logger.info("%s → (%.6f, %.6f)", addr2, lat2, lon2)

        save_coords_to_txt(lat1, lon1, lat2, lon2, INPUT_FILE)
    else:
        if not os.path.isfile(INPUT_FILE):
            logger.error("Не найден входной файл: %s", INPUT_FILE)
            sys.exit(1)

    logger.info("Чтение координат из %s", INPUT_FILE)
    logger.info("Запуск C-обработчика...")

    if not run_c_processor(INPUT_FILE, OUTPUT_FILE):
        logger.error("Ошибка выполнения C-кода")
        sys.exit(1)

    if not os.path.isfile(OUTPUT_FILE):
        logger.info("Не найден выходной файл: %s", OUTPUT_FILE)
        sys.exit(1)

    path = read_path_from_csv(OUTPUT_FILE)

    logger.info("Прочитан путь из %d точек", len(path))
    logger.info("Отрисовка карты...")

    draw_map(path)

    logger.info("Готово!")

if __name__ == "__main__":
    main()
