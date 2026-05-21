"""
Generate synthetic data for middleware evaluation bassed on the data which can be produced 
by CARLA Simulator. Currently supports the generation of data for the following sensors:
- RGB cameras
- LiDARs
- IMUs
- GNSS receivers
- Radar

@author: Mario Martín <martinperezm@unican.es>
@version: 0.2
"""

import json
import argparse
import numpy as np

MAX_PRECISION = 15
MIN_PRECISION = 1

def parse_args():
    parser = argparse.ArgumentParser(
        description='Creates a synthetic dataset emulating CARLA sensors.'
    )
    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument(
        '--minutes', '-m',
        type = int,
        help = 'Time in minutes.'
    )
    group.add_argument(
        '--frames', '-f',
        type = int,
        help = 'Number of frames'
    )
    parser.add_argument(
        '--precision', '-p',
        type = int,
        default = 6,
        choices = range(MIN_PRECISION, MAX_PRECISION + 1),
        metavar = f'[{MIN_PRECISION}, {MAX_PRECISION}]',
        help = 'Float precision (64b) or number of decimals.'
    )
    parser.add_argument(
        '--output', '-o',
        default = 'dataset.json',
        help = 'Name of JSON File.'
    )
    parser.add_argument(
        '--interactive', '-i',
        action = 'store_true',
        help = 'Use interactive mode'
    )

    args = parser.parse_args()

    if not args.interactive and args.minutes is None and args.frames is None:
        parser.error('Either --minutes/-m or --frames/-f is required (unless using --interactive/-i)')

    return args

def args_to_settings(args):
    if args.minutes is not None:
        num_frames = args.minutes * 60
    else:
        num_frames = args.frames
    filename = args.output
    if not filename.endswith('.json'):
        filename += '.json'

    return num_frames, args.precision, filename

def ask_int(prompt, min_val = None, max_val = None):
    """
    Helper to verify an integer from the user.
    """
    while True:
        try:
            value = int(input(f'  >>> {prompt} ' ))
            if min_val and value < min_val:
                print(f'  [!] Minimum value: {min_val}')
                continue
            if max_val and value > max_val:
                print(f'  [!] Maximum value: {max_val}')
                continue
            return value
        except ValueError:
            print('  [!] Plese, enter a valid value.')

def ask_str(prompt, default = None):
    """
    Helper to ask for a string from the user.
    """
    if default:
        default_str = f'  [{default}]'
    else:
        default_str = ''
   
    value = input(f'  >>> {prompt}{default_str} ').strip()
    return value or default
    
def interactive_tune():
    """

    """
    print('\n' + '='*30)
    print('  GENERATOR SETTINGS')
    print('='*30 + '\n')

    # Minutes/Frames selector
    print('Select time unit:')
    print('  [1] Time in minutes (experimental - 1 fps)')
    print('  [2] Number of frames')
    sel = ask_int('Option [1-2]:', 1, 2)

    if sel == 1:
        minutes = ask_int('Minutes [1-59]:', 1, 59)
        num_frames = minutes * 60
        print(f'  [+] Settings: {minutes} min --> {num_frames} frames\n')
    else:
        num_frames = ask_int('Frames [>0]:', 1)
        print(f'  [+] Settings: {num_frames} frames\n')

    # Precision
    print('Decimal precision:')
    print(f'  Range: {MIN_PRECISION}-{MAX_PRECISION}')
    precision = ask_int(f'Precision {MIN_PRECISION}-{MAX_PRECISION}:', MIN_PRECISION, MAX_PRECISION)
    example = round(np.random.uniform(MIN_PRECISION, MAX_PRECISION), precision)
    print(f'  [+] Example: {example}\n')

    # Filename
    print('JSON Filename:')
    print('  (.json extension will be added automatically)')
    filename = ask_str('File:', 'dataset')
    if not filename.endswith('.json'):
        filename += '.json'
    print(f'  [+] Save in: ./{filename}\n')

    # Settings
    print('=' * 30)
    print('  SETTINGS')
    print('=' * 30)
    print(f'  * Frames:    {num_frames} frames')
    print(f'  * Precision: {precision} decimals')
    print(f'  * Filename:  {filename}')
    print('=' * 30)

    return num_frames, precision, filename

def gen_dec_val(precision):
    return round(np.random.uniform(10.0, 99.9), precision)

def generate_synthetic_data(num_frames, precision, filename):
    """
    Generate synthetic data for the supported sensors. The data is generated based on the 
    sensor type and the user's input. The generated data is saved in a JSON file.
    """
    print('Starting simulation with: ')
    print(f'--> {num_frames} frames.')
    print(f'--> {precision} decimales.')
    print(f'--> Exporting to: {filename}')
    print('-' * 40 + '\n')

    dataset = []

    for frame in range(num_frames):
        frame_data = {
            'frame': int(frame),
            'sensors': {
                'gnss': {
                    'latitude': gen_dec_val(precision),
                    'longitude':gen_dec_val(precision),
                    'altitude': gen_dec_val(precision)
                },
                'imu': {
                    'accelerometer': {
                        'x': gen_dec_val(precision),
                        'y': gen_dec_val(precision),
                        'z': gen_dec_val(precision)
                    },
                    'gyroscope': {
                        'x': gen_dec_val(precision),
                        'y': gen_dec_val(precision),
                        'z': gen_dec_val(precision)
                    },
                    'compass': gen_dec_val(precision)
                },
                'CameraRGB': {
                    'x': 1920,
                    'y': 1080,
                    'fov': 90,
                    'frameURI': f'C:/Vehicle/rgb_{frame:08d}.png'
                },
                'Radar': {
                    'num_detections': int(np.random.randint(10, 99)),
                    'cloudUri': f'C:/Vehcile/radar_cloud_{frame:08d}.bin'
                },
                'LiDAR': {
                    'num_points': int(np.random.randint(10, 99)),
                    'horizontal_angle': 20,
                    'cloudUri': f'C:/Vehicle/lidar_cloud_{frame:08d}.bin'
                }
            }
        }
        dataset.append(frame_data)
    
    with open(filename, 'w') as f:
        json.dump(dataset, f, indent=4)

    print('Dataset generated.')

if __name__ == "__main__":
    args = parse_args()

    if args.interactive:
        num_frames, precision, filename = interactive_tune()
    else:
        num_frames, precision, filename = args_to_settings()

    generate_synthetic_data(num_frames, precision, filename)
