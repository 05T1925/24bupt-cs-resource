from __future__ import annotations

import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "screenshots" / "00_environment"
TSHARK = Path(r"C:\Program Files\Wireshark\tshark.exe")
WIRESHARK = Path(r"C:\Program Files\Wireshark\Wireshark.exe")
FONT_PATH = Path(r"C:\Windows\Fonts\msyh.ttc")


def command_output(command: list[str]) -> str:
    result = subprocess.run(command, capture_output=True, text=True, errors="replace")
    return (result.stdout + result.stderr).strip()


def render(title: str, command: str, body: str, output: Path) -> None:
    width, height = 1600, 1000
    image = Image.new("RGB", (width, height), "#101418")
    draw = ImageDraw.Draw(image)
    title_font = ImageFont.truetype(str(FONT_PATH), 32)
    label_font = ImageFont.truetype(str(FONT_PATH), 22)
    body_font = ImageFont.truetype(str(FONT_PATH), 20)

    draw.rectangle((0, 0, width, 76), fill="#20262d")
    draw.text((32, 20), title, font=title_font, fill="#f4f7fa")
    draw.text((34, 100), f"> {command}", font=label_font, fill="#73d0ff")

    y = 150
    for line in body.splitlines():
        if y > height - 34:
            break
        draw.text((34, y), line, font=body_font, fill="#e3e8ee")
        y += 28

    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def wlan_ipconfig_section(text: str) -> str:
    marker = "Wireless LAN adapter WLAN:"
    start = text.find(marker)
    if start == -1:
        return text
    section = text[start:]
    next_adapter = section.find("\nEthernet adapter ", len(marker))
    if next_adapter != -1:
        section = section[:next_adapter]
    lines = [
        line
        for line in section.splitlines()
        if "Lease Obtained" not in line and "Lease Expires" not in line
    ]
    return "\n".join(lines).strip()


def main() -> int:
    wireshark_version = command_output([str(WIRESHARK), "-v"]).splitlines()[0]
    tshark_interfaces = (PROJECT_ROOT / "exports" / "tshark_interfaces.txt").read_text(
        encoding="utf-8-sig"
    )
    ipconfig = (PROJECT_ROOT / "exports" / "ipconfig_before.txt").read_text(
        encoding="utf-8-sig"
    )

    render(
        "Wireshark 版本检查",
        '"C:\\Program Files\\Wireshark\\Wireshark.exe" -v',
        wireshark_version,
        OUTPUT_DIR / "01_wireshark_version.png",
    )
    render(
        "TShark 抓包接口列表",
        '"C:\\Program Files\\Wireshark\\tshark.exe" -D',
        tshark_interfaces,
        OUTPUT_DIR / "02_tshark_interfaces.png",
    )
    render(
        "当前 WLAN 的 IP 配置",
        "ipconfig /all",
        wlan_ipconfig_section(ipconfig),
        OUTPUT_DIR / "03_ipconfig_all.png",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
