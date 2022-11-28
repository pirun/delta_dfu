import os
import sys

paths = "binaries/signed_images"
patch_path = "binaries/patches/patch"
#command = "detools create_patch --compression heatshrink binaries/signed_images/source.bin binaries/signed_images/target.bin binaries/patches/patch1.bin"
command = "detools create_patch --compression heatshrink "
source_version = ''
target_version = ''

def padded_hex(s,p):
    return '0x' + s[2:].zfill(p)

def create_patch_file(path):
    global command
    global patch_path
    global source_version
    global target_version
    source_count = 0
    target_count = 0
    files = os.listdir(path)
    for file_name in files:
        #print("file_name="+file_name)
        if "source_" in file_name:
            source_count += 1
        elif "target_" in file_name:
            target_count += 1
           
    if(source_count != 1):
        print("Multiple source files:%d!!!\r\n" %source_count)
    if(target_count != 1):
        print("Multiple target files:%d!!!\r\n" %target_count)

    for file_name in files:
        if "source_" in file_name:
            source_path = os.path.normpath(os.path.join(path, file_name))
            source_version = file_name.split("_")[1].split(".bin")[0]
            print("source file:%s  source_version:%s  source_path:%s"  %(file_name,source_version,source_path))
        elif "target_" in file_name:
            target_path = os.path.normpath(os.path.join(path, file_name))
            target_version = file_name.split("_")[1].split(".bin")[0]
            print("target file:%s  target_version:%s  target_path:%s"  %(file_name,target_version,target_path))

    patch_path += "_from_" + source_version + "_to_" + target_version + ".bin"
    command += source_path + ' ' + target_path + ' ' + patch_path
    print(patch_path)
    os.system(command)



def sign_patch_file(sign_path):
    print("patch_path:%s" %patch_path)
    size=os.stat(patch_path).st_size 
    f=open(patch_path,'r+b')
    contents = f.read()
    f.seek(0)
    f.write('NEWP'.encode()) 
    f.write(size.to_bytes(4,byteorder='little'))
    f.write(source_version.encode())
    #f.write(target_version.encode())
    f.write(contents)

    with open(sign_path,encoding='utf-8') as file_obj:
        text = file_obj.read()
        sign_command = text.strip('\r\n') + ' ' + patch_path + ' ' + patch_path.replace("patch_from","signed_patch_from")
        print(sign_command)
        os.system(sign_command)


'''
def start(path,max_size,input_header_size):
    header_size = max(input_header_size,8)
    size=os.stat(path).st_size 
    f=open(path,'r+b')
    contents = f.read()
    f.seek(0)
    f.write('NEWP'.encode()) 
    f.write(size.to_bytes(header_size-4,byteorder='little'))
    f.write(contents)

    print("Patch size: " + hex(size) + " + " + hex(header_size) + " (header)")

    if((max_size-header_size)<size):
        print("WARNING: Patch too large for patch partition!")
'''

if __name__ == "__main__":
    create_patch_file(paths)
    sign_patch_file("scripts/signature.py")
    
    #start(sys.argv[1],int(sys.argv[2],0),int(sys.argv[3],0))