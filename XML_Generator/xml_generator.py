"""
XML Dynamic Type Generator for FastDDS, parsing an existing IDL file.

@author Mario Martin-Perez <martinperezm@unican.es>
@version 0.2
"""

import argparse
import re
import xml.etree.ElementTree as ET
from xml.dom import minidom

def indent_xml(elem: ET.Element):
    """
    Applies a clean identation to an XML element tree.

    Args:
        elem (ET.Element): The root element of the XML tree to format.

    Returns:
        str: A properly indented, multi-line XML strin representation using tabs.
    """
    rough_string = ET.tostring(elem, 'utf-8')
    parsed = minidom.parseString(rough_string)
    return parsed.toprettyxml(indent = '\t')

def map_idl_to_xml(idl_type: str):
    """
    Maps a native IDL data type to its corresponding FastDDS XML representation.

    This function handles both primitive types and complex structures. If the type is a bounded string, it
    extracts the maximum allowed length. Unrecognized types default to nested complex structures.

    Args:
        idl_type (str): The raw data type string extracted from the IDL file.

    Returns:
        tuple: A tuple containing:
            - xml_type (str): The FastDDS-compilant XML type attribute value.
            - non_basic_type (str or None): The name of the nested structure if 'nonBasic', otherwise None.
            - max_len (str or None): The maximum size string if the type is a bounded string, otherwise None.
    """
    idl_type = idl_type.strip()

    string_match = re.match(r'string<(\d+)>', idl_type) # Detect strings with length limit
    if string_match is not None:
        return 'string', None, string_match.group(1)
    
    match idl_type:
        case 'string':
            return 'string', None, None
        case 'long' | 'int32':
            return 'int32', None, None
        case 'double':
            return 'double', None, None 
        case 'boolean':
            return 'boolean', None, None 
        case 'short' | 'int16':
            return 'int16', None, None
        case _:
            return 'nonBasic', idl_type, None 

def parse_idl_to_dict(idl_path: str):
    """
    Parses an IDL file to extract the module namespace and structured type definitions.

    The function reads the file content, strips out single-line and multi-line comments, and uses
    regular expressions to capture the module name and all defined structs.
    Each struct's members are mapped out into type-name pairs.

    Args:
        idl_path (str): The path to the source IDL specification file.

    Returns:
        tuple: A tuple containing:
            - module_name (str): The extracted name of the IDL module namespace. Defaults to 'SyntheticData'
                if not found.
            - structs (dict[str, list[dict[str, str]]]): A dictionary mapping each structure to a list
            of its fields, where each field is represented as {'name': field_name, 'type': field_type}.
    """
    with open(idl_path, 'r', encoding = 'utf-8') as f:
        content = f.read()

    content = re.sub(r'//.*', '', content) # Remove comments
    content = re.sub(r'/\*.*?\*/', '', content, flags = re.DOTALL)

    # Module name
    module_match = re.search(r'module\s+(\w+)\s*\{', content)
    module_name = module_match.group(1) if module_match is not None else 'SyntheticData'

    # Find struct blocks (name and contents):
    struct_blocks = re.findall(r'struct\s+(\w+)\s*\{(.*?)};', content, flags = re.DOTALL)

    structs = {}

    for struct_name, contents in struct_blocks:
        fields = list()
        field_lines = re.findall(r'([\w<>]+)\s+(\w+)\s*;', contents)
        for field_type, field_name in field_lines:
            fields.append({'name': field_name, 'type': field_type})
        structs[struct_name] = fields
    
    return module_name, structs

def generate_xml_from_idl(input_idl, output_xml):
    """
    Translates parsed IDL structures into a valid FastDDS Dynamic Types XML file.

    This function coordinates the end-to-end XML generation pipeline. It maps structural
    hierarchies into standard '<types>' tags and configures necessary XML member attributes such
    as 'nonBasicTypeName' or 'stringMaxLength'.

    Args:
        input_idl (str): Path to the source IDL file.
        output_xml (str): Path and filename of the destination XML configuration file.

    Returns:
        None: The function writes the generated XML payload directly to disk.
    """
    module_name, structs = parse_idl_to_dict(input_idl)

    if not structs:
        print(f'[!] No valid structures found.')
        return
    
    root_elem = ET.Element('dds', {
        'xmlns': 'http://www.eprosima.com',
        'version': '1.0'
    })

    # Types section
    types_elem = ET.SubElement(root_elem, 'types')

    for struct_name, fields in structs.items():
        struct_node  = ET.SubElement(types_elem, 'struct', name = struct_name)

        for field in fields:
            xml_type, non_basic_type, max_len = map_idl_to_xml(field['type'])

            member_attrs = {'name': field['name'], 'type': xml_type}
            if xml_type == 'nonBasic' and non_basic_type is not None:
                member_attrs['nonBasicTypeName'] = non_basic_type
            elif xml_type == 'string' and max_len is not None:
                member_attrs['stringMaxLength'] = max_len
            
            ET.SubElement(struct_node, 'member', **member_attrs)

    # Profiles section - Participants and topics
    #TODO if needed for benchmark test.

    xml_string = indent_xml(root_elem)
    with open(output_xml, 'w', encoding = 'utf-8') as f:
        f.write(xml_string)

    print(f'[+] Dynamic types XML generated succesfuly from IDL: {output_xml}')
    print(f'\t- Detected module: {module_name}')

def main():
    """
    Execution entry point for the FastDDS XML Dynamic Type Generator.
    """
    parser = argparse.ArgumentParser(description='Generate FastDDS XML Dynamic Types from an existing IDL file.')
    parser.add_argument('--input', '-i', default='IDL_Generator/data_types.idl', help='Input IDL file generated by idl_generator.py.')
    parser.add_argument('--out', '-o', default='XML_Generator/dynamic_types_profiles.xml', help='Output XML file.')
    args = parser.parse_args()
    
    generate_xml_from_idl(args.input, args.out)

if __name__ == '__main__':
    main()