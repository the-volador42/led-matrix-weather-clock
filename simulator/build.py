"""Сборка симулятора: компилирует эффекты в WebAssembly и вкладывает
результат прямо в simulator.html (страница открывается двойным кликом).

Нужен компилятор:  python -m pip install ziglang
Запуск:            python build.py          (или двойной клик по build.bat)
"""
import base64
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
SKETCH = HERE.parent / "led_matrix_weather_clock"


def effect_sources():
    """Файлы скетча, в которых только графика (без Wi-Fi, погоды и веба)."""
    names = ["matrix.cpp", "scene.cpp", "state.cpp", "effects.cpp"]
    files = [SKETCH / n for n in names] + sorted(SKETCH.glob("fx_*.cpp"))
    missing = [f.name for f in files if not f.exists()]
    if missing:
        raise SystemExit("Не найдены файлы скетча: " + ", ".join(missing))
    return files


def main() -> int:
    wasm = HERE / "sim.wasm"
    cmd = [
        sys.executable, "-m", "ziglang", "c++",
        "-target", "wasm32-wasi", "-mexec-model=reactor",
        "-O2", "-std=c++17", "-s",
        "-I", str(HERE / "shim"),
        str(HERE / "sim_main.cpp"),
        *[str(f) for f in effect_sources()],
        "-o", str(wasm),
    ]
    if wasm.exists():
        wasm.unlink()  # чтобы не открыть случайно старую сборку
    print("Компилирую эффекты...")
    res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    # Предупреждения libc++ не интересны, показываем только ошибки компиляции.
    errors = [line for line in (res.stdout + res.stderr).splitlines() if " error: " in line or "error:" in line[:12]]
    if res.returncode != 0 or not wasm.exists():
        print("\n".join(errors) or (res.stdout + res.stderr)[-4000:])
        print("\nОШИБКА: сборка не удалась.")
        return 1

    template = (HERE / "viewer.html").read_text(encoding="utf-8")
    b64 = base64.b64encode(wasm.read_bytes()).decode("ascii")
    out = HERE / "simulator.html"
    out.write_text(template.replace("__WASM_BASE64__", b64), encoding="utf-8")
    print(f"Готово: {out.name} ({out.stat().st_size // 1024} КБ). Открой его в браузере.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
