"""Generate CJK glyphs for Wi-Fi names; Pillow and Windows Microsoft YaHei."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 14)
data = bytearray()
# These ranges and their order are mirrored by ecology_name.h.
for start, end in [(0x3000, 0x303f), (0x4e00, 0x9fff), (0xff00, 0xffef)]:
    for code in range(start, end+1):
        image = Image.new('1', (16, 20))
        ImageDraw.Draw(image).text((0, 0), chr(code), font=font, fill=1)
        for y in range(20):
            for x in (0, 8):
                data.append(sum(bool(image.getpixel((x+i,y))) << (7-i) for i in range(8)))
(root/'firmware/wireless-ecology/main/name_font.bin').write_bytes(data)
print('SSID font bytes:', len(data))
