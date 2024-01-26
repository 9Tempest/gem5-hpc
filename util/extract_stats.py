import json
import re

def convert_to_json(file_path, output_file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    # Ignore the first two lines
    lines = lines[2:]

    data = {}
    for line in lines:
        # Check for the end indicator line
        if '---------- End Simulation Statistics ----------' in line:
            break

        if not line.strip():
            continue

        # Splitting each line into key and value parts
        key, value = line.split(maxsplit=1)
        if '#' in value:
            value, description = value.split('#', 1)
            value_numbers = re.findall(r'[\d\.\-e]+', value)
        else:
            description = ""
            value_numbers = re.findall(r'[\d\.\-e]+', value)

        # Parsing the numerical values
        if len(value_numbers) == 1:
            # Single value
            value_num = value_numbers[0]
        else:
            # Multiple values
            value_num = value_numbers

        # Building the nested dictionary structure
        key_parts = key.split('.')
        current_level = data
        for part in key_parts[:-1]:
            if part not in current_level:
                current_level[part] = {}
            current_level = current_level[part]

        current_level[key_parts[-1]] = {'val': value_num, 'description': description.strip()}

    # Write the JSON data to the specified output file
    with open(output_file_path, 'w') as output_file:
        json.dump(data, output_file, indent=2)

# Example usage
file_path = '/data3/gem5/m5out/stats.txt'
output_file_path = '/data3/gem5/m5out/stats.json'

json_data = convert_to_json(file_path, output_file_path)
