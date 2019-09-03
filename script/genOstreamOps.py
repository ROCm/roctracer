#!/usr/bin/python

import os, sys, re
import CppHeaderParser

rx_dict = {
    'struct_name': re.compile(r'typedef (?P<struct_name>.*)\n'),
    'field_type': re.compile(r'\s+name\[raw_type\]=(?P<field_type>.*)\n'),
    'field_name': re.compile(r'\s+name\[name\]=(?P<field_name>.*)\n'),

}

def _parse_line(line):

    for key, rx in rx_dict.items():
        match = rx.search(line)
        if match:
            return key, match
    return None, None

def parse_file(filepath):
    f= open("ostream.h","w+")
    with open(filepath, 'r') as file_object:
        line = file_object.readline()
        field_type='no_type'
        flag=0
        while line:
            key, match = _parse_line(line)
            if key == 'struct_name':
                if flag == 1:
                    f.write("    return out;\n")
                    f.write("}\n")
                struct_name = match.group('struct_name')
                #print struct_name
                f.write("template <>\n")
                f.write("struct output_streamer<"+struct_name+"&> {\n")
                f.write("inline static std::ostream& put(std::ostream& out, "+struct_name+"& v)\n")
                f.write("{\n")
                flag=1;
            if key == 'field_type':
                field_type = match.group('field_type')
                if field_type == "":
                    field_type="notype"
            if key == 'field_name':
                field_name = match.group('field_name')
                if field_name == "":
                    field_name="noname"
                #print "\t"+field_type+"\t"+field_name
                f.write("    roctracer::kfd_support::output_streamer<"+field_type+">::put(out,v."+field_name+");\n")
            line = file_object.readline()
    f.close()
    print ("File ostream.h has been generated.")
    return 

def gen_cppheader_lut(filepath):
    try:
        cppHeader = CppHeaderParser.CppHeader(filepath)
    except CppHeaderParser.CppParseError as e:
        print(e)
        sys.exit(1)

    f= open("cppheader_lut.txt","w+")
    for c in cppHeader.classes:
        f.write("typedef %s\n"%(c))
        for l in range(len(cppHeader.classes[c]["properties"]["public"])):
            for key in cppHeader.classes[c]["properties"]["public"][l].keys():
                f.write("	name[%s]=%s\n"%(key,cppHeader.classes[c]["properties"]["public"][l][key]))
    f.close()
    print ("File cppheader_lut.txt has been generated.")
    return


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        gen_cppheader_lut(arg)
        parse_file("cppheader_lut.txt")
        
