import json
import urllib2
import os

def get_largest_matrix_in_group():
    f = open("matrices.json", 'r').read()
    matrix_groups = json.loads(f)

    for mg in matrix_groups:
        ms = mg['matrices']
        ms.sort(key=lambda m: m['num_nonzeros'])
        yield ms[-1]

def get_middle_matrix_in_group():
    f = open("matrices.json", 'r').read()
    matrix_groups = json.loads(f)

    for mg in matrix_groups:
        ms = mg['matrices']
        ms.sort(key=lambda m: m['num_nonzeros'])
        yield ms[len(ms)/2]

def download_file(url, group, dstdir):
    file_name = group + '-' + url.split('/')[-1]
    u = urllib2.urlopen(url)
    f = open(os.path.join(dstdir, file_name), 'wb')
    meta = u.info()
    file_size = int(meta.getheaders("Content-Length")[0])
    print "Downloading: %s Bytes: %s" % (file_name, file_size)
    
    file_size_dl = 0
    block_sz = 8192
    while True:
        buffer = u.read(block_sz)
        if not buffer:
            break
    
        file_size_dl += len(buffer)
        f.write(buffer)
        status = r"%10d  [%3.2f%%]" % (file_size_dl, file_size_dl * 100. / file_size)
        status = status + chr(8)*(len(status)+1)
        print status,
    print 
    f.close()

for m in get_middle_matrix_in_group():
    dstdir = 'matrices'
    if not os.path.exists(dstdir):
        os.mkdir(dstdir)
    f = download_file(m['mm_file'], m['group'], dstdir)

