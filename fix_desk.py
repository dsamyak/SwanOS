import re
with open('src/desktop.c', 'r') as f: text = f.read()

text = text.replace('xFF000000xFF000000', 'xFF000000')
text = text.replace('10xFF000000', '10')
text = text.replace('30xFF000000', '30')
text = text.replace('500xFF000000', '500')

text = re.sub(r'vga_set_palette\([^;]+;', '// palette', text)

with open('src/desktop.c', 'w') as f: f.write(text)
