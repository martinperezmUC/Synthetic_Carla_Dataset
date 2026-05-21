"""
Generate synthetic data for middleware evaluation bassed on the data which can be produced 
by CARLA Simulator. Currently supports the generation of data for the following sensors:
- RGB cameras
- LiDARs
- IMUs
- GNSS receivers
- Radar

@author: Mario Martín <martinperezm@unican.es>
@version: 0.1
"""

import json
import os
import numpy as np

CURRENT_VERSION = 0.1
WELCOME_MESSAGE = ('Welcome to the CARLA-like synthetic data generation tool.'
    '\nYou will now see some options to personalize the format of your data. '
    'Please, take into account, that this data is completely random and is '
    'for pure benchmarking tests only.\n'
    )

MAX_DECIMALS = 30

num_frames = 0
num_decimals = 0
filename = ''

def welcome():
    """
    Welcome the user, describe the program and ask if the user desires a default and simple configuration.
    """
    print(WELCOME_MESSAGE)
    
    print('Author: Mario Martin-Perez <martinperezm@unican.es>')
    print(f'Version: {CURRENT_VERSION}\n\n')
    
def tune_generation():
    print('+ What unit of measurement do you wish to use in order to set the number of frames to generate?:')
    print('\t1) Time in minutes (experimental - only 1 frame per second supported).')
    print('\t2) Number of frames.')
    print('\tChoose [1-2]:')
    sel_1 = int(input())

    match (sel_1):
        case 1:
            print('\tTime in minutes selected. Please, indicate the time [1-10]:')
            time = int(input())
            if (time < 1 or time > 10):
                print('\tTime is out of [1-10] bounds.')
                exit()
            else:
                num_frames = time * 60

        case 2:
            print('\tNumber of frames selected. Please, indicate the number [1-inf)')
            num_frames = int(input())
            if (num_frames > 0):
                print(f'\tNumber of frames: {num_frames}')
            else:
                print('\tInvalid value.')
                exit()

        case _:
            print(f'Selection = {sel_1}. Closing...')
            exit()

    print('+ How many decimal characters do you want [1, 30]?:')
    num_decimals = int(input())
    if (num_decimals < 1 or num_decimals > MAX_DECIMALS):
        print('\tDecimals are out of [1-30] bounds.')
        exit()
    else: 
        print(f'\tNumber of decimals: {num_decimals}. Example: {round(np.random.uniform(1,10), num_decimals)}')

    print('Please, choose a filename for your json (.json will be added automatically). Choose it carefully, ' +
          'since this is the last user input allowed. \n IMPORTANT: NO FORMAT VERIFICATION IS MADE!')
    filename = input() + '.json'
    print(f'Data will be located on ./{filename}')

def gen_dec_val():
    return round(np.random.uniform(10.0, 99.9), num_decimals)

def generate_synthetic_data():
    """
    Generate synthetic data for the supported sensors. The data is generated based on the 
    sensor type and the user's input. The generated data is saved in a JSON file.
    """
    print('Starting simulation with: ')
    print(f'--> {num_frames} frames.')
    print(f'--> {num_decimals} decimales.')
    print(f'--> Exporting to: {filename}')
    print('-' * 40 + '\n')

    dataset = []

    for frame in range(num_frames):
        frame_data = {
            'frame': int(frame),
            'sensors': {
                'gnss': {
                    'latitude': gen_dec_val(),
                    'longitude':gen_dec_val(),
                    'altitude': gen_dec_val()
                },
                'imu': {
                    'accelerometer': {
                        'x': gen_dec_val(),
                        'y': gen_dec_val(),
                        'z': gen_dec_val()
                    },
                    'gyroscope': {
                        'x': gen_dec_val(),
                        'y': gen_dec_val(),
                        'z': gen_dec_val()
                    },
                    'compass': gen_dec_val 
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
                    'cloudUri': f'C:Vehicle/lidar_cloud_{frame:08d}.bin'
                }
            }
        }
        dataset.append(frame_data)
    
    with open(('./' + filename), 'w') as f:
        json.dump(dataset, f, indent=4)

    print('Dataset generated.')

if __name__ == "__main__":
    welcome()
    tune_generation()
    generate_synthetic_data()
