"""
Generate synthetic data for middleware evaluation bassed on the data which can be produced 
by CARLA Simulator. Currently supports the generation of data for the following sensors:
- RGB cameras
- LiDARs
- IMUs
- GNSS receivers
- Radar

@author: Mario Martín <martinperezm@unican.es>
@version: 0.4
"""

import json
import argparse
import numpy as np

MAX_PRECISION = 15
MIN_PRECISION = 1

def parse_args():
    """
    Parses and validates command-line arguments.

    Configures 'argparse' to handle parameters required for creating a synthetic dataset that
    emulates CARLA simulator sensors.

    Returns:
        argparse.Namespace: An object containing the validated attributes:
            - minutes (int): Simulation time in minutes (mutually exclusive with 'frames').
            - frames (int): Number of frames to generate (mutually exclusive with 'minutes').
            - precision (int): Decimal precision for floats (default: 6).
            - output (str): Output JSON filename (default: 'dataset.json').
            - interactive (bool): Flag to enable interactive mode.
            - sensors (str): Comma-separated list of active sensors (default: 'all').
            - lat_int (int): Integer digits for latitude (default: 2).
            - long_int (int): Integer digits for longitude (default: 2).
            - alt_int (int): Integer digits for altitude (default: 3).
            - compass_int (int): Integer digits for compass (default: 3).
            - accel_int (int): Integer digits for accelerometer values (default: 2).
            - gyro_int (int) Integer digits for gyroscope values (default: 3).

    Raises:
        SystemExit: If neither '--minutes' nor '--frames' is provided, unless '--interactive'
            mode is enabled. Also raised automatically by argparse if argument types or
            choices constraints are violated.
    """
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
    parser.add_argument(
        '--sensors', '-s',
        type = str,
        default = 'all',
        help = 'Comma-separated list of sensors gnss,imu,camera,radar,lidar (default: all)'
    )
    parser.add_argument(
        '--lat-int',
        type = int,
        default = 2,
        help = 'Number of integer digits for latitude.'
    )
    parser.add_argument(
        '--long-int',
        type = int,
        default = 2,
        help = 'Number of integer digits for longitude.'
    )
    parser.add_argument(
        '--alt-int',
        type = int,
        default = 3,
        help = 'Number of integer digits for altitude.'
    )
    parser.add_argument(
        '--compass-int',
        type = int,
        default = 3,
        help = 'Number of integer digits for compass.'
    )
    parser.add_argument(
        '--accel-int',
        type = int,
        default = 2,
        help = 'Number of integer digits for accelerometer values'
    )
    parser.add_argument(
        '--gyro-int',
        type = int,
        default = 3,
        help = 'Number of integer digits for gyroscope values'
    )

    args = parser.parse_args()

    if not args.interactive and args.minutes is None and args.frames is None:
        parser.error('Either --minutes/-m or --frames/-f is required (unless using --interactive/-i)')

    return args

def args_to_settings(args: argparse.Namespace):
    """
    Converts parsed command-line arguments into structured configuration settings.

    This function processes the raw arguments, calculates the total number of frames if runtime
    was given in minutes, ensures the output file has the correct extension and groups sensor-specific
    integer digit constraints.

    Args:
        args (argparse.Namespace): The parsed comman-line arguments containing simulation configuration
            parameters.

    Returns:
        tuple: A tuple containing the processed settings in the following order:
            - num_frames (int or None): Total number of frames to generate. If 'args.minutes' was provided,
            it is converted to frames. Otherwise, it defaults to 'args.frames'.
            - precision (int): Decimal precision for floating-point values.
            - filename (str): Properly formatted JSON output filename (ensured to end with '.json').
            - sensors (set): Processed sensor list returned by 'parse_sensors()'.
            - int_digits (dict[str, int]): A dictionary mapping sensor attributes ('lat', 'lon', 'alt',
            'compass', 'accel', 'gyro') to their respective maximum integer digit lengths.
    """
    if args.minutes is not None:
        num_frames = args.minutes * 60
    else:
        num_frames = args.frames
    filename = args.output
    if not filename.endswith('.json'):
        filename += '.json'
    sensors = parse_sensors(args.sensors)
    int_digits = {
        'lat': args.lat_int,
        'lon': args.lon_int,
        'alt': args.alt_int,
        'compass': args.compass_int,
        'accel': args.accel_int,
        'gyro': args.gyro_int
    }

    return num_frames, args.precision, filename, sensors, int_digits

def ask_int(prompt: str, min_val:int = None, max_val: int = None):
    """
    Prompts the user for an integer input and validates it against optional bounds.

    This function repeatedly requests user input until a valid integer is entered.
    If optional minimum or maximum boundaries are provided, it ensures the input
    falls within the specified range before returning.

    Args:
        prompt (str): The message displayed to the user when requesting input.
        min_val (int, optional): The inclusive minimum allowable value. Defaults to None.
        max_val (int, optional): The inclusive maximum allowable value. Defaults to None.

    Returns:
        int: The validated integer entered by the user that satisfies all constraints.
    """
    while True:
        try:
            value = int(input(f'  >>> {prompt} ' ))
            if min_val is not None and value < min_val:
                print(f'  [!] Minimum value: {min_val}')
                continue
            if max_val is not None and value > max_val:
                print(f'  [!] Maximum value: {max_val}')
                continue
            return value
        except ValueError:
            print('  [!] Plese, enter a valid value.')

def ask_str(prompt: str, default:str = None):
    """
    Prompts the user for a string input with an optional default value.

    This function requests a string from the user via the command line. If a default
    value is provided, it is displayed alongside the prompt. It the user presses
    Enter without typing anything, the default value is returned.

    Args:
        prompt (str): The message displayed to the user when requesting input.
        default (str, optional): The value to return if the user provides empty input.
            Defaults to None.

    Returns:
        str: The trimmed string entered by the user, or the 'default' value if the input
            was empty.
    """
    if default:
        default_str = f'  [{default}]'
    else:
        default_str = ''
   
    value = input(f'  >>> {prompt}{default_str} ').strip()
    return value or default

def parse_sensors(sensors_str: str):
    """
    Parses and validates a comma-separated string of sensor names.

    Converts the input string into a set of unique sensor names. If the input is 'all',
    it returns all available sensors. If the user inputs invalid sensors, it prints a
    warning and falls back to enabling all valid sensors.

    Args:
        sensors_str (str): A comma-separated string of sensors or the keyboard 'all'.

    Returns:
        set[str]: A set of unique, lowercased and stripped sensor names that are
            validated againsst the supported sensor list.
    """
    if sensors_str.lower() == 'all':
        return {'gnss', 'imu', 'camera', 'radar', 'lidar'}
    
    valid_sensors = {'gnss', 'imu', 'camera', 'radar', 'lidar'}
    enabled = set(s.strip().lower() for s in sensors_str.split(','))

    invalid = enabled - valid_sensors

    if invalid:
        print(f"  [!] Invalid sensors: {', '.join(invalid)}")
        print(f"  [+] Valid sensors: {','.join(valid_sensors)}")
        return valid_sensors
    
    return enabled if enabled else valid_sensors

def ask_sensors_interactive():
    """
    Displays an interactive menu for the user to select which CARLA sensors to simulate.

    The user can choose a single sensor by its number, type '0' or 'all' to select everything,
    or provide a comma-separated list of numers (e.g., '1,3,4') to enable specific sensors
    simultaneously. If no valid selection is made, it defaults to selecting all sensors.

    Returns:
        set[str]: A set containing the unique, lowercased names of the selected sensors.
    """
    sensors = {'gnss', 'imu', 'camera', 'radar', 'lidar'}
    selected = set()

    # Select sensors
    print('Select sensors to generate:')
    print('  [1] GNSS')
    print('  [2] IMU')
    print('  [3] Camera RGB')
    print('  [4] Radar')
    print('  [5] LiDAR')
    print('  [0] All sensors')

    choice = ask_str('Sensors [0-5]:', '0').strip()

    if choice == '0' or choice.lower() == 'all':
        selected = sensors 
    else:
        sensor_map = {
            '1': 'gnss',
            '2': 'imu',
            '3': 'camera',
            '4': 'radar',
            '5': 'lidar'
        }

        for digit in choice.split(','):
            digit = digit.strip()
            if digit in sensor_map:
                selected.add(sensor_map[digit])
    
    return selected if selected else sensors

def interactive_tune():
    """
    Runs an interactive CLI wizard to configure CARLA synthetic dataset generator.

    This function guides the user step-by-setp through setting up the simulation parameters
    via terminal prompts. It handles time/frame allocations, floating-point precision, target
    output filename, sensor selection, and integer digit constraints for data formatting.
    At the end, it prints a summary of the chosen configuration.

    Returns:
        tuple: A tuple containing the fully configured settings in the following order:
            - num_frames (int): Total number of simulation frames to generate.
            - precision (int): Decimal precision for floating-point values.
            - filename (str): Target output filename guaranteed to end in '.json'.
            - sensors (set[str]): A set of selected sensor names to simulate.
            - int_digits (dict[str, int]): A dictionary mapping sensor attributes
                ('lat', 'lon', 'alt', 'compass', 'accel', 'gyro') to their respective
                maximum integer digit lengths.
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

    # Select sensors
    sensors = ask_sensors_interactive()

    # Integer digits
    print('Integer digits for sensor values (only positive ranges):')
    lat_int = ask_int('Latitude integer digits [1-2]:', 1, 2)
    lon_int = ask_int('Longitude integer digits [1-3]:', 1, 3)
    alt_int = ask_int('Altitude integer digits [1-4]:', 1, 4)
    compass_int = ask_int('Compass integer digits [1-3]', 1, 3)
    accel_int = ask_int('Accelerometer integer digits [1-2]:', 1, 2)
    gyro_int = ask_int('Gyroscope integer digits [1-3]:', 1, 3)

    int_digits = {
        'lat': lat_int,
        'lon': lon_int,
        'alt': alt_int,
        'compass': compass_int,
        'accel': accel_int,
        'gyro': gyro_int
    }

    # Settings
    print('=' * 30)
    print('  SETTINGS')
    print('=' * 30)
    print(f'  [+] Frames:    {num_frames} frames')
    print(f'  [+] Precision: {precision} decimals')
    print(f'  [+] Filename:  {filename}')
    print(f'  [+] Sensors:   {', '.join(sorted(sensors))}')
    print(f'  [+] Digits:    lat={lat_int}, long={lon_int}, alt={alt_int}, compass={compass_int}, accel={accel_int}, gyro={gyro_int}')
    print('=' * 30)

    return num_frames, precision, filename, sensors, int_digits

def gen_bounded_value(min_val, max_val, int_digits, precision):
    max_integer = 10 ** int_digits - 1

    if min_val < 0:
        lower = max(min_val, -max_integer)
    else:
        lower = max(min_val, 0)

    upper = min(max_val, max_integer)

    value = np.random.uniform(lower, upper)
    return round(float(value), precision)

def generate_synthetic_data(num_frames, precision, filename, enabled_sensors, int_digits):
    """
    Generate synthetic data for the supported sensors. The data is generated based on the 
    sensor type and the user's input. The generated data is saved in a JSON file.
    """
    print('Starting simulation with: ')
    print(f'--> {num_frames} frames.')
    print(f'--> {precision} decimals.')
    print(f'--> Exporting to: {filename}')
    print(f'--> Sensors: {', '.join(sorted(enabled_sensors))}')
    print(f'--> Integer digits: {int_digits}')
    print('-' * 40 + '\n')

    dataset = []

    for frame in range(num_frames):
        frame_data = {
            'frame': int(frame),
            'sensors': {}
        }

        # GNSS
        if 'gnss' in enabled_sensors:
            frame_data['sensors']['gnss'] = {
                'latitude': gen_bounded_value(0.0, 90.0, int_digits['lat'], precision),
                'longitude': gen_bounded_value(0.0, 180.0, int_digits['lon'], precision),
                'altitude': gen_bounded_value(0.0, 9000.0, int_digits['alt'], precision)
            }
        
        # IMU
        if 'imu' in enabled_sensors:
            frame_data['sensors']['imu'] = {
                'accelerometer': {
                    'x': gen_bounded_value(0.0, 16.0, int_digits['accel'], precision),
                    'y': gen_bounded_value(0.0, 16.0, int_digits['accel'], precision),
                    'z': gen_bounded_value(0.0, 16.0, int_digits['accel'], precision)
                },
                'gyroscope': {
                    'x': gen_bounded_value(0.0, 360.0, int_digits['gyro'], precision),
                    'y': gen_bounded_value(0.0, 360.0, int_digits['gyro'], precision),
                    'z': gen_bounded_value(0.0, 360.0, int_digits['gyro'], precision)
                },
                'compass': gen_bounded_value(0.0, 360.0, int_digits['compass'], precision)
            }
        
        # Camera RGB
        if 'camera' in enabled_sensors:
            frame_data['sensors']['CameraRGB'] = {
                'x': 1920,
                'y': 1080,
                'fov': 90,
                'frameURI': f'C:/Vehicle/rgb_{frame:08d}.png'
            }
        
        # Radar
        if 'radar' in enabled_sensors:
            frame_data['sensors']['Radar'] = {
                'num_detections': int(np.random.randint(10, 99)),
                'cloudUri': f'C:/Vehicle/radar_cloud_{frame:08d}.bin'
            }

        if 'lidar' in enabled_sensors:
            frame_data['sensors']['LiDAR'] = {
                'num_points': int(np.random.randint(10,99)),
                'horizontal_angle': 20,
                'cloudUri': f'C:/Vehicle/lidar_cloud_{frame:08d}.bin'
            }
        dataset.append(frame_data)
    
    with open(filename, 'w') as f:
        json.dump(dataset, f, indent=4)

    print('Dataset generated.')

if __name__ == "__main__":
    args = parse_args()

    if args.interactive:
        num_frames, precision, filename, sensors, int_digits = interactive_tune()
    else:
        num_frames, precision, filename, sensors, int_digits = args_to_settings()

    generate_synthetic_data(num_frames, precision, filename, sensors, int_digits)
