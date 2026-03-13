with open('src/desktop.c', 'r') as f: text = f.read()

text = text.replace('0x88000000xFF000000', '0x88000000')
text = text.replace('0x00000000xFF000000', '0x00000000')
text = text.replace('0xFF000000xFF000000', '0xFF000000')

with open('src/desktop.c', 'w') as f: f.write(text)
