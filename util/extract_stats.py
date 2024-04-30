import json
import re
import argparse

def convert_to_json(file_path, output_file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()

    data = {}
    for line in lines:
        # Check for the end indicator line
        if '---------- End Simulation Statistics ----------' in line:
            break
        
        if "----------" in line:
            continue

        if not line.strip():
            continue

        # Splitting each line into key and value parts
        try:
            key, value = line.split(maxsplit=1)
        except ValueError:
            print(f"Error in line: {line}, Error: {ValueError}")
            continue

        if '#' in value:
            value, description = value.split('#', 1)
            value_numbers = re.findall(r'[\d\.\-e]+', value)
        else:
            description = ""
            value_numbers = re.findall(r'[\d\.\-e]+', value)

        if "nan" in value:
            continue

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

        current_level[key_parts[-1]] = {'val': value_num, 'description': description.strip(), "count": 1}
    return data

def filter_stats(stats, interest_list):
    filtered_stats = {}
    for key in stats:
        if key in interest_list:
            filtered_stats[key] = stats[key]
        elif isinstance(stats[key], dict):
            tmp = filter_stats(stats[key], interest_list)
            if tmp:
                filtered_stats[key] = tmp
    return filtered_stats

def merge_and_sum_dicts(dict1, dict2):
    # Helper function to add two items
    def add_items(item1, item2):
        if isinstance(item1, dict) and isinstance(item2, dict):
            # Recurse into nested dictionaries
            return merge_and_sum_dicts(item1, item2)
        else:
            # If not dictionaries, assume they are integers or floats and add them
            return float(item1) + float(item2)

    # Merge two dictionaries
    merged_dict = {}
    # Combine all keys from both dictionaries
    all_keys = set(dict1.keys()) | set(dict2.keys())

    for key in all_keys:
        if key in dict1 and key in dict2 and key != "description":
            # If key is in both dictionaries, add the items
            merged_dict[key] = add_items(dict1[key], dict2[key])
        elif key in dict1:
            # If key is only in the first dictionary, add it to the result
            merged_dict[key] = dict1[key]
        else:
            # If key is only in the second dictionary, add it to the result
            merged_dict[key] = dict2[key]

    return merged_dict

def merge_stats(stats):
    merged_stats = {}
    if not isinstance(stats, dict):
        return stats
    for key in stats:
        if "cpu" in key and (key[-1] == "0" or key[-1] == "1" or key[-1] == "2" or key[-1] == "3"):
            new_key = key[:-1]
            if new_key not in merged_stats:
                merged_stats[new_key] = merge_stats(stats[key])
            else:
                # print(f"key: {key}, new_key: {new_key}")
                # print(f"stats[{key}]: {stats[key]}, merged_stats[{new_key}]: {merged_stats[new_key]}")
                merged_stats[new_key] = merge_and_sum_dicts(merged_stats[new_key], merge_stats(stats[key]))
        else:
            merged_stats[key] = merge_stats(stats[key])
    return merged_stats

# Example usage
# input = '/data3/gem5/tests/test-progs/spmmv/m5out/stats.txt'
# output = '/data3/gem5/tests/test-progs/spmmv/m5out/stats.json'
if __name__ == '__main__':
    #Take arguments for the file path and output
    parser = argparse.ArgumentParser(description='Convert stats.txt to JSON and filter based on interests.')
    parser.add_argument('--input', type=str, help='Path to the input stats.txt file.')
    parser.add_argument('--output', type=str, help='Path to the output JSON file.')

    args = parser.parse_args()

    interest_list = ["cpi",
                     "loadToUse::mean"]

    tmp = ["demandHits::switch_cpus",
          "demandMisses::switch_cpus",
          "demandAccesses::switch_cpus",
          "demandMissRate::switch_cpus",
          "demandAvgMissLatency::switch_cpus",
          "demandMshrHits::switch_cpus",
          "demandMshrMisses::switch_cpus",
          "demandMshrAccesses::switch_cpus",
          "demandMshrMissRate::switch_cpus",
          "demandAvgMshrMissLatency::switch_cpus"]
    
    for core in range(0, 4):
        for item in tmp:
            interest_list.append(f"{item}{core}")

    json_data = convert_to_json(args.input, args.output)
    json_data = filter_stats(json_data, interest_list)
    merged_json_data = merge_stats(json_data)

    # Write the JSON data to the specified output file
    with open(args.output, 'w') as output_file:
        json.dump(json_data, output_file, indent=2)
        json.dump(merged_json_data, output_file, indent=2)
    
    latency = merged_json_data['system']['switch_cpus']['lsq0']['loadToUse::mean']
    print(f"{latency['val'] / latency['count']}", end=',')
    print(f"{merged_json_data['system']['cpu']['dcache']['demandAccesses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['dcache']['demandHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['dcache']['demandMisses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['dcache']['demandMshrHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['dcache']['demandMshrMisses::switch_cpus']['data']['val']}", end=',')
    latency = merged_json_data['system']['cpu']['dcache']['demandAvgMissLatency::switch_cpus']['data']
    print(f"{latency['val'] / latency['count'] / 333.0000}", end=',')
    print(f"{merged_json_data['system']['cpu']['l2cache']['demandAccesses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['l2cache']['demandHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['l2cache']['demandMisses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['l2cache']['demandMshrHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['cpu']['l2cache']['demandMshrMisses::switch_cpus']['data']['val']}", end=',')
    latency = merged_json_data['system']['cpu']['l2cache']['demandAvgMshrMissLatency::switch_cpus']['data']
    print(f"{latency['val'] / latency['count'] / 333.0000}", end=',')
    print(f"{merged_json_data['system']['l3']['demandAccesses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['l3']['demandHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['l3']['demandMisses::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['l3']['demandMshrHits::switch_cpus']['data']['val']}", end=',')
    print(f"{merged_json_data['system']['l3']['demandMshrMisses::switch_cpus']['data']['val']}", end=',')
    latency = merged_json_data['system']['l3']['demandAvgMshrMissLatency::switch_cpus']['data']
    print(f"{latency['val'] / latency['count'] / 333.0000}", end=',')
    print()
