"""Test-only fixtures. No external encoder is used by the player itself."""
import sys
import subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / '.deps'))
import imageio_ffmpeg

root = Path(__file__).parent / 'thumbnail-fixtures'
root.mkdir(exist_ok=True)
ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
for name, size, sar in [('横屏', '640x360', '1'), ('竖屏', '180x320', '1'), ('变形像素', '720x576', '64/45')]:
    subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i',
                    f'testsrc2=size={size}:rate=12:duration=8', '-vf', f'setsar={sar}',
                    '-c:v', 'mpeg4', '-q:v', '3', str(root / (name+'.mp4'))], check=True)
(root / '损坏.mp4').write_bytes(b'not a video')
subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i',
                'sine=frequency=440:duration=5', str(root / 'audio.wav')], check=True)
(root / '横屏.srt').write_text('1\n00:00:00,000 --> 00:00:08,000\nThumbnail subtitle test\n', encoding='utf-8')
print(root)
