# Return the number of test failures from the last CDash submission.
import sys
import json

json_file = 'out.json'
try:
    with open(json_file) as data_file:
        try:
            data = json.load(data_file)
        except ValueError:
            print('ERROR:Invalid json file')
            sys.exit(1)
except IOError:
    print('ERROR:cannot open '+json_file)
    sys.exit(1)

# Pick the last one.
sys.exit(data['buildgroups'][0]['builds'][-1]['test']['fail'])

