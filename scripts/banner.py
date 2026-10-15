"""Create the matching Pixel Doodle HOME Menu banner and silent audio."""
import pathlib, sys, wave
from PIL import Image
out=pathlib.Path(sys.argv[1])
logo=Image.open(pathlib.Path(__file__).resolve().parents[1]/'assets/icon-3ds.png').convert('RGB')
banner=Image.new('RGB',(256,128),'#211039')
banner.paste(logo.resize((96,96),Image.Resampling.NEAREST),(80,16))
banner.save(out/'banner.png')
with wave.open(str(out/'banner.wav'),'wb') as f:
    f.setnchannels(1);f.setsampwidth(2);f.setframerate(22050);f.writeframes(bytes(44100))
