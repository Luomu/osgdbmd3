#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <iostream>

static osg::Node* load_md3(const char* filename,
                           const osgDB::ReaderWriter::Options* options);

class ReaderWriterMD3 : public osgDB::ReaderWriter
{
public:
    ReaderWriterMD3()
    {
        supportsExtension("md3", "Quake3 model");
    }

    virtual const char* className() const {
        return "Quake MD3 Reader";
    }

    virtual ReadResult readNode(const std::string& filename,
                                const osgDB::ReaderWriter::Options* options) const;
};

REGISTER_OSGPLUGIN(md3, ReaderWriterMD3);

osgDB::ReaderWriter::ReadResult
ReaderWriterMD3::readNode (const std::string& file,
                           const osgDB::ReaderWriter::Options* options) const
{
    std::string ext = osgDB::getLowerCaseFileExtension(file);
    if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

    std::string fileName = osgDB::findDataFile( file, options );
    if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

    // code for setting up the database path so that internally referenced file are searched for on relative paths. 
    osg::ref_ptr<Options> local_opt = options ? static_cast<Options*>(options->clone(osg::CopyOp::SHALLOW_COPY)) : new Options;
    local_opt->setDatabasePath(osgDB::getFilePath(fileName));

    return load_md3(fileName.c_str(), options);
}

typedef struct {
    int ident;
    int version;
    char name[64];
    int flags;
    int num_frames;
    int num_tags;
    int num_surfaces;
    int num_skins;
    int ofs_frames;
    int ofs_tags;
    int ofs_surfaces;
    int ofs_eof;
} MD3_HEADER;

#define MD3_HEADER_MAGIC 0x33504449 // "IDP3"
// this one was wrong?: http://icculus.org/~phaethon/q3a/formats/md3format.html
//#define MD3_HEADER_MAGIC  0x51806873

// struct frame

// struct tag

// struct surface

// struct shader

// struct triangle

// texcoord

// vertex

static void dumpHeaderInfo(MD3_HEADER* h)
{
    using namespace std;
    //~ cout << " " << h-> << endl;
    cout << "Ident: " << h->ident << endl;
    cout << "Version: " << h->version << endl;
    cout << "Name: " << h->name << endl;
    cout << "Flags: " << h->flags << endl;
    cout << "Num_frames: " << h->num_frames << endl;
    cout << "Num_tags: " << h->num_tags << endl;
    cout << "Num_surfaces: " << h->num_surfaces << endl;
    cout << "Num_skins: " << h->num_skins << endl;
    cout << "ofs_frames: " << h->ofs_frames << endl;
    cout << "ofs_surfaces:" << h->ofs_surfaces << endl;
    cout << "ofs_eof: " << h->ofs_eof << endl;
}

static osg::Node*
load_md3(const char* filename, const osgDB::ReaderWriter::Options* options)
{
    struct stat st;
    void* mapbase;
    int file_fd;
    osg::Node* result = 0;

    if(stat(filename, &st) < 0)
        return 0;

    file_fd = open(filename, O_RDONLY);
    if(file_fd <= 0)
        return 0;

    mapbase = malloc(st.st_size);
    if(read(file_fd, mapbase, st.st_size) == 0) {
        close(file_fd);
        return 0;
    }

    MD3_HEADER* md3_header = (MD3_HEADER*)mapbase;
    if(md3_header->ident != MD3_HEADER_MAGIC || md3_header->version < 15) {
        free(mapbase);
        close(file_fd);
        return 0;
    }

    dumpHeaderInfo(md3_header);

    free(mapbase);
    close(file_fd);
    return result;
}
