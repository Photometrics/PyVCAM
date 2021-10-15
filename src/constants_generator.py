###############################################################################
# File: constants_generator.py
# Author: Cameron Smith
# Date of Last Edit: June 2, 2017
#
# Purpose: Generate the necessary constants used in pvcam.h into a reusable
#          python module.
#
# Notes: No formal testing exists for this script. Please report all bugs to
#        to whoever is in charge of this project currently.
#
# Bugs: Enums are not created as Enum objects. Any struct that assigns to an
#       enum will result in a NameError as the Enum does not exist; only its
#       entities. Update occurrences manually to be a ctypes C type until fix.
#       (Known occurrence(s): md_ext_item_info [struct]. Currently patched by
#       assigning values to be of type `void`.
###############################################################################
import os
import re
from datetime import date

pvcam_sdk_path = os.environ["PVCAM_SDK_PATH"]
pvcam = r'{}inc/pvcam.h'.format(pvcam_sdk_path)
constants = r'./pyvcam/constants.py'


def define_writer(match):
    """Constructs a string representing defines in header files.

    Parameters:
        match: The matched pattern from the regular expression containing the
               named groups of the variable name and its associated value.
    """

    return match.group('var'), match.group('val')


def enum_writer(match, infile):
    """Constructs a tuple containing the data to represent enums in python.

    The tuple will have the form (<String>, <List>), where the string will
    contain the comment block denoting which enum the following list is from,
    and the list contains tuples pairing variables to values.

    Paramters:
        match: The matched pattern from the regular expression containing the
               name of the current enum.
        infile: The file being read from.
    """

    enums = []
    position = 0  # The current position inside of `enums`
    enum_group = match.group('name')

    # Skip two lines ahead to ignore the '{' line
    next(infile)
    line = next(infile)
    line = remove_comment(line, infile)

    # Set up the regular expressions to find name and values of enums
    enum_name = '\s+(?P<name>\w+)'
    enum_val = enum_name + '\s*=(?P<val>[^,]*)'

    # Loop through the file until the end of the current enum is reached.
    # If the current line matches the pattern <name> = <val>, then append
    # a tuple onto `enums` that pairs <name> with <val>. If the current
    # line does not match that pattern, check if it matches the pattern
    # <name>. If it does, append a tuple to `enums` pairing <name> with
    # the previous position's <name> + 1. If position = 0, then it is the
    # first element in the enum, so pair <name> with 0 instead. If the
    # line does not match either of these patters, move to the next line.
    while line != '}\n':
        val = re.search(enum_val, line)
        if val:
            enums.append((val.group('name'), val.group('val')))
        else:
            name = re.search(enum_name, line)
            if name and position == 0:
                enums.append((name.group('name'), '0'))
            elif name:
                enums.append((name.group('name'),
                             enums[position - 1][0] + ' + 1')) # Prev. var. + 1
            # Offset the increment to position if nothing is being added
            else: position -= 1
        line = next(infile)
        line = remove_comment(line, infile)
        position += 1
    return (enum_group, enums)


def struct_writer(infile):
    """Builds a tuple containing the data to represent structs in python.

    Parameters:
        match: The matched pattern from the regular expression containing the
               name of the current enum.
        infile: The file being read from.

    Returns:
        A tuple in the form (<name of struct>, [list of fields])
    """

    # Maps the data types used in in the header file to the ctypes equivalent.
    types = {'uns32': 'ctypes.c_uint32', 'uns16': 'ctypes.c_uint16',
             'uns64': 'ctypes.c_uint64', 'uns8': 'ctypes.c_uint8',
             'int32': 'ctypes.c_int32', 'int16': 'ctypes.c_int16',
             'int64': 'ctypes.c_int64', 'int8': 'ctypes.c_int8',
             'long64': 'ctypes.c_long', 'char*': 'ctypes.c_char_p',
             'void*': 'ctypes.c_void_p', 'flt64': 'ctypes.c_float',
             'rs_bool': 'ctypes.c_short', 'char': 'ctypes.c_char',
             'void': 'ctypes.c_void_p'}

    fields = []
    # Skip two lines ahead to ignore the '{' line
    next(infile)
    line = next(infile)
    line = remove_comment(line, infile)

    struct_pattern = '^\s+(const )?(?P<type>\w+\*?)\s+(?P<var>\w+)\[?(?P<num>\d+)?\]?;$'

    while '}' not in line:
        match = re.search(struct_pattern, line)
        if match:
            # EAFP
            try:
                var_type = types[match.group('type')]
            except KeyError:
                if match.group('type')[-1] == '*' and match.group('type')[:-1] in types:
                    var_type = types[match.group('type')[:-1]]
                else:
                    var_type = types['void'] # Handle bug for md_ext_item_info
            variable = match.group('var')
            if match.group('num'):
                var_type += ' * {}'.format(match.group('num'))
            if var_type[-1] == '*':
                var_type = 'ctypes.POINTER({})'.format(var_type[:-1])
            fields.append((variable, var_type))
        line = next(infile)
        line = remove_comment(line, infile)

    line = next(infile)
    line = remove_comment(line, infile)
    struct_name = '^(?P<name>\w+);$'
    name = re.search(struct_name, line)
    return name.group('name'), fields


def remove_comment(to_remove, infile):
    """Removes trailing block comments from the end of a string.

    Parameters:
        to_remove: The string to remove the comment from.
        infile: The file being read from.

    Returns:
        The paramter string with the block comment removed (if comment was
        present in string).
    """
    start_comment = re.search('\s*(\/\*|//)', to_remove)

    # Remove comments if they are in the matched group.
    if start_comment:
        end_comment = re.search('.*\*\/', to_remove)
        if end_comment or ('//' in to_remove and not '/*' in to_remove) :
            removed = to_remove[:start_comment.start(0)] + '\n'
            return removed
        while not end_comment:
            to_remove = next(infile)
            end_comment = end_comment = re.search('.*\*\/', to_remove)
        return ''
    else:
        removed = to_remove
    return removed


def parse_line(line, infile, collection):
    """Determines if a line is a define or a start of an enum or struct.

    After determining the structure of a current line, parse_line will then
    call the appropriate funcion in order to deal with the current line.

    Parameters:
        line: The current line being read in from the input file.
        infile: The opened input file.
        collection: The dictionary containing all the lists of data.
    """
    # Define, enum, and struct check pattern.
    define = '^#define (?P<var>\w+)\s+(?P<val>.+)$'
    enum = '^typedef enum (?P<name>\w+)$'
    struct = '^typedef struct .+$'

    expressions = {'defines': define, 'enums': enum, 'structs': struct}

    for key in expressions:
        match = re.search(expressions[key], line)
        if match:
            if key == 'defines':
                collection[key].append(define_writer(match))
            if key == 'enums':
                collection[key].append(enum_writer(match, infile))
            if key == 'structs':
                collection[key].append(struct_writer(infile))




if __name__ == '__main__':
    header_comment = \
"""###############################################################################
# File: constants.py
# Author: Cameron Smith
# Date of Last Edit: {}
#
# Purpose: To maintain the naming conventions used with PVCAM.h for Python
#          scripts.
#
# Notes: This file is generated by constants_generator.py. See that script for
#        details about implementation. Please do not alter this file. Instead,
#        make any additional changes to the constants_generator.py if
#        additional data is needed.
#
# Bugs: [See constants_generator.py]
###############################################################################\n"""\
    .format(date.today())

    defines = []
    macros = []
    enums = []
    structs = []
    collection = {'defines': defines, 'macros': macros, 'enums': enums,
                  'structs': structs}
    # Open file and read parse each line before writing
    try:
        with open(pvcam, 'r') as infile:
            for line in infile:
                line = remove_comment(line, infile)
                parse_line(line, infile, collection)

    except FileNotFoundError:
        print(pvcam + " not found. Make sure that the PVCAM SDK is installed "
              "on this machine.")
        exit(-1)

    with open(constants, 'w') as outfile:
        outfile.write(header_comment)
        outfile.write('import ctypes,os\n')
        outfile.write('### DEFINES ###\n')
        for tup in collection['defines']:
            outfile.write('{} = {}\n'.format(tup[0], tup[1]))
        outfile.write('\n### ENUMS ###\n')
        for tup in collection['enums']:
            outfile.write('\n# {}\n'.format(tup[0]))
            for item in tup[1]:
                outfile.write('{} = {}\n'.format(item[0], item[1]))
        outfile.write('\n### STRUCTS ###\n')
        for tup in collection['structs']:
            outfile.write('class {}(ctypes.Structure):\n'.format(tup[0]))
            outfile.write('    _fields_ = [\n')
            for item in tup[1]:
                outfile.write('                (\'{}\', {}),\n'.format(item[0],
                                                                       item[1]))
            outfile.write('               ]\n\n')

    # Diagnostic
    if False:
        print('Defines: {}'.format(len(collection['defines'])))
        print('Enums: {}'.format(len(collection['enums'])))
        for tup in collection['enums']:
            print('---{}: {}'.format(tup[0].ljust(20), len(tup[1])))
        print('Structs: {}'.format(len(collection['structs'])))
        for item in collection['structs']:
            print('---{}: {}'.format(item[0], item[1]))
