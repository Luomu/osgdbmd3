#include <osgDB/ReadFile>

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
ReaderWriterMD2::readNode (const std::string& file,
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

#define MD3_HEADER_MAGIC  0x51806873 // "IDP3"

// struct frame

// struct tag

// struct surface

// struct shader

// struct triangle

// texcoord

// vertex

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
    if(md3_header->magic != MD3_HEADER_MAGIC || md3_header->version < 15) {
        munmap(mapbase);
        free(mapbase);
        close(file_fd);
        return 0;
    }

    free(mapbase);
    close(file_fd);
    return result;
}
