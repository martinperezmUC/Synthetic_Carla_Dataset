"""
IDL generator for FastDDS, using a existing dataset.
Use:
  python idl_generator.py --input dataset.json --module SyntheticData --out dataset.idl

Basic type inferetion and nested structures.

@author: Mario Martín <martinperezm@unican.es>
@version: 0.1
"""
import argparse
import json
from collections import OrderedDict

class TypeRegistry:
    """
    Class for types registry.
    """
    def __init__(self):
        """
        Initialize attributes of the class.
        """
        # name -> definition (OrderedDict of fields)
        self.structs = OrderedDict()
        self.seen_names = set()

    def unique_name(self, base: str):
        """
        Save unique name in PascalCase format and return it.

        Args:
            base (str): Default name to be transformed.

        Returns:
            str: Unique name generated from initial name.
        """
        cleaned = base.replace('_', ' ').replace('-', ' ')
        cleaned = ''.join(ch if ch.isalnum() else ' ' for ch in cleaned)
        name = cleaned.title().replace(' ', '')

        if name and name[0].isdigit(): # If sensor name is a number or begins with it
            name = "Sensor" + name

        # Detect duplicates
        orig = name
        i = 1
        while name in self.seen_names:
            name = f"{orig}{i}"
            i += 1
        self.seen_names.add(name)
        return name

    def register_struct(self, name: str, fields: OrderedDict):
        """
        Save structure.

        Args:
            name (str): Structure name.
            fields (OrderedDict): Structure fields in an ordered dictionary.

        Returns:
            str: Name of the registered structure.
        """
        # fields: OrderedDict of fieldname -> type (string) or ('struct', struct_name)
        if name in self.structs: # If repeated, update with new fields
            # Merge fields (keep broader types as string)
            existing = self.structs[name]
            for k, v in fields.items():
                if k not in existing:
                    existing[k] = v
            return name
        self.structs[name] = fields
        return name

def map_primitive(value: bool | int | float | str, len: int = None):
    """
    Map Python primitive to IDL type. String if unknown. String<len> should me recommended for benchmark.

    Args:
        value (bool | int | float | str): Value to map.
        len (int, optional): If the value is a string, it could come with its length. Defaults to None.

    Returns:
        str: Name or string representation of the IDL mapping.
    """
    if isinstance(value, bool):
        return 'boolean'
    if isinstance(value, int):
        return 'long'
    if isinstance(value, float):
        return 'double'
    if isinstance(value, str):
        if len is not None:
            return f'string<{int(len)}>'
        return 'string'
    return 'string'

def infer_struct(name_base: str, obj: dict, registry: TypeRegistry, path: tuple = (), lens: dict = None):
    """
    Recursively infers IDL structure fields from a sample dictionary.

    This function parses a dictionary representing a sensor or data entry.
    If a nested dictionary is found, it recursively creates a sub-structure.
    Primitives are mapped to their corresponding IDL types.

    Args:
        name_base (str): The base name for the structure being inferred.
        obj (dict): The sample data dictionary to inspect.
        registry (TypeRegistry): The global type registry to save inferred structures.
        path (tuple, optional): The sequence of keys leading to the current object
                                Used for string length lookups. Defaults to ().
        lens (dict, optional): A dictionary mapping field paths to their maximum string lengths.
                            Defaults to None.

    Returns:
        str: Unique and final name of the registered structure.
    """
    fields = OrderedDict()
    for k, v in obj.items():
        current_path = path + (k,)
        if isinstance(v, dict):
            nested_name = registry.unique_name(k)
            infer_struct(nested_name, v, registry, pat = current_path, lens=lens)
            fields[k] = ('struct', nested_name)
        else:
            # primitive (or numeric string)
            cur_path = tuple(list(path) + [k])
            len = None
            if lens is not None:
                len = lens.get(cur_path)
            fields[k] = map_primitive(v, len=len)
    registry.register_struct(name_base, fields)
    return name_base


def generate_idl(module_name: str, registry: TypeRegistry, top_struct_name: str = 'Frame'):
    """
    Serializes the registered type structures into a valid FastDDS IDL string.

    This function iterates through all structures stored in the type registry
    and formats them into Interface Definition Language (IDL) syntax. It wraps
    the structures inside a defined IDL module namespace and ensures that a top-level
    encapsulating structure (by default, 'Frame') is included, creating a default
    fallback if it was not previously registered.

    Args:
        module_name (str): The namespace or module name for the IDL file.
        registry (TypeRegistry): The type registry containing the inferred structures.
        top_struct_name (str, optional): The name of the main encapsulating structure that contains all fields.
                                        Defaults to 'Frame'.

    Returns:
        str: A fully formatted, multi-line IDL specification string ready to be
            written to a file.
    """
    lines = []
    lines.append('// IDL Specification generated by idl_generator.py')
    lines.append(f'module {module_name} {{')

    # Write structs in registration order
    for name, fields in registry.structs.items():
        lines.append(f'\tstruct {name} {{')
        for fname, ftype in fields.items():
            if isinstance(ftype, tuple) and ftype[0] == 'struct':
                lines.append(f'\t{ftype[1]} {fname};')
            else:
                lines.append(f'\t{ftype} {fname};')
        lines.append('  };')
        lines.append('')

    # Ensure top-level Frame exists
    if top_struct_name not in registry.structs:
        # create a default Frame with a frame index
        registry.register_struct(top_struct_name, OrderedDict([('frame', 'int32')]))
        lines.append(f'\tstruct {top_struct_name} {{')
        lines.append('\tint32 frame;')
        lines.append('  };')
        lines.append('')

    lines.append('};')
    idl_spec = '\n'.join(lines)
    return idl_spec


def sensors_to_structs(sensors_dict: dict, registry: TypeRegistry, lens: dict = None):
    """
    Transforms a catalog of sensor examples into registered IDL structures.

    This function processes a dictionary of sample sensor data. For each sensor, it generates
    a unique structure name and determines if the sensor contains nested fields (a dictionary)
    or a raw primitive value. Complex sensors are delegated to recursive inference, while
    primitive sensors are automatically wrapped in a dedicated single-field structure to comply
    with IDL requirements.

    Args:
        sensors_dict (dict): A mapping of sensor names to their corresponding sample data objects.
                            Example: 'gps: {lat: 0.0}, counter: 42}'.
        registry (TypeRegistry): _description_
        lens (dict, optional): _description_. Defaults to None.
    """
    # sensors_dict: mapping sensor_name -> example_object
    for sensor_name, example in sensors_dict.items():
        struct_name = registry.unique_name(sensor_name)
        if isinstance(example, dict):
            infer_struct(struct_name, example, registry, path=(sensor_name,), lens=lens)
        else:
            # primitive sensor -> wrap in a struct with one value field
            registry.register_struct(struct_name, OrderedDict([('value', map_primitive(example))]))

def record_lens(prefix: tuple, obj: dict | str, lens: dict):
    """
    Recursively traverses a data structure to record the length of leaf string values.

    This function walks through nested dictionaries to find final primitive values.
    When it encounters a string, it stores its character length in the provided 'lens'
    dictionary, using the full path sequence of keys (as a tuple) as the unique identifier.
    It mutates the 'lens' dictionary in place and avoids overwriting existing paths.


    Args:
        prefix (tuple): A sequence of keys representing the current path leading to the
                        object. Example: ('imu', 'accelerometer').
        obj (dict | str): The current object or data snippet being inspected.
        lens (dict): The shared dictionary where calculated string lengths are stored
                    as reference lookups.
    """
    if isinstance(obj, dict):
        for k, v in obj.items():
            record_lens(prefix + (k,), v, lens)
    elif isinstance(obj, str): # Final value. Check if str and record length
        if prefix not in lens:
            lens[prefix] = len(obj)

def main():
    """
    Main execution pipeline to generate a FastDDS IDL file froma JSON dataset.

    This function coordinates the entire generation process through the following steps:
    1. Parses command-line arguments for input/output files and module naming.
    2. Iterates through the JSON dataset to extract unique sensor examples and compute string lengths
    via 'record_lens'.
    3. Translates sensor definitions into IDL-compatible structures using 'sensors_to_structs'.
    4. Constructs a top-level encapsulating 'Frame' structure that references all sensors.
    5. Serializes the final type registry into IDL syntax and writes it to the output file.
    """
    parser = argparse.ArgumentParser(description = 'Generate IDL for FastDDS based on dataset.json file.')
    parser.add_argument('--input', '-i', default = 'dataset.json', help = 'Input JSON file.')
    parser.add_argument('--module', '-m', default = 'SyntheticData', help = 'IDL Module name.')
    parser.add_argument('--out', '-o', default = 'dataset.idl', help = 'Output IDL file.')
    args = parser.parse_args()

    with open(args.input, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Collect sensor examples and compute string lengths
    sensors_examples = {}
    lens = {}

    # Read sensor types and data example of each one
    for entry in data:
        sensors = entry.get('sensors', {}) # Considerate bug with registries with not 'sensors' key
        for sensor_name, sensor_val in sensors.items():
            if sensor_name not in sensors_examples:
                sensors_examples[sensor_name] = sensor_val
            record_lens((sensor_name,), sensor_val, lens)

    registry = TypeRegistry()

    # create sensor structs (pass maxlens and flags)
    sensors_to_structs(sensors_examples, registry, lens=lens)

    # create Frame struct referencing sensors
    frame_fields = OrderedDict()
    frame_fields['frame'] = 'int32'
    for sensor_name in sensors_examples.keys():
        struct_name = None
        # find registered struct matching sensor (by starting name)
        for reg_name in registry.structs.keys():
            if reg_name.lower().startswith(sensor_name.lower()):
                struct_name = reg_name
                break
        if struct_name is None:
            struct_name = registry.unique_name(sensor_name)
            registry.register_struct(struct_name, OrderedDict())
        frame_fields[sensor_name] = ('struct', struct_name)

    registry.register_struct('Frame', frame_fields)

    idl_text = generate_idl(args.module, registry, top_struct_name='Frame')

    with open(args.out, 'w', encoding='utf-8') as f:
        f.write(idl_text)

    print(f'IDL generado en {args.out}')


if __name__ == '__main__':
    main()
