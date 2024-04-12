import json
import re
import argparse

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
    return data

def filter_stats(stats, interest_list):
    filtered_stats = {}
    for key in stats:
        if key in interest_list:
            filtered_stats[key] = stats[key]
        elif isinstance(stats[key], dict):
            tmp =  filter_stats(stats[key], interest_list)
            if tmp:
                filtered_stats[key] = tmp
    return filtered_stats

# Example usage
# file_path = '/data3/gem5/tests/test-progs/spmmv/m5out/stats.txt'
# output_file_path = '/data3/gem5/tests/test-progs/spmmv/m5out/stats.json'
if __name__ == '__main__':
    #Take arguments for the file path and output
    parser = argparse.ArgumentParser(description='Convert stats.txt to JSON and filter based on interests.')
    parser.add_argument('file_path', type=str, help='Path to the input stats.txt file.')
    parser.add_argument('output_file_path', type=str, help='Path to the output JSON file.')

    args = parser.parse_args()

    interest_list = ["cpi",
                     "overallAccesses::total", "overallMshrHits::total", "overallMshrMisses::total",
                    "overallMissRate::total", "overallMshrMissRate",
                     "overallAvgMshrMissLatency::total", 
                     "peakBW",
                     "busUtil","busUtilRead", "busUtilWrite", "avgWrQLen", "avgRdQLen", "requestorReadAvgLat::switch_cpus0.data"]

    json_data = convert_to_json(args.file_path, args.output_file_path)
    json_data = filter_stats(json_data, interest_list)

    # Write the JSON data to the specified output file
    with open(args.output_file_path, 'w') as output_file:
        json.dump(json_data, output_file, indent=2)
