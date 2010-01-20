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

struct vec {
    vec() : x(0.0f), y(0.0f), z(0.0f) {}
    vec(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float x;
    float y;
    float z;
};

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

typedef struct {
    vec bbmin;
    vec bbmax;
    vec origin;
    float radius;
    char name[16];
} MD3_FRAME;

typedef struct {
    char name[64];
    vec origin;
    float axis[3][3];
} MD3_TAG;

typedef struct {
    int ident;
    char name[64];
    int flags;
    int num_frames;
    int num_shaders;
    int num_verts;
    int num_triangles;
    int ofs_triangles;
    int ofs_shaders;
    int ofs_st;
    int ofs_xyznormal;
    int ofs_end;
} MD3_SURFACE;

typedef struct {
    char name[64];
    int shader_index;
} MD3_SHADER;

typedef struct {
    int indexes[3];
} MD3_TRIANGLE;

typedef struct {
    float uv[2];

    void dumpInfo() {
        std::cout << uv[0] << " " << uv[1] << std::endl;
    }
} MD3_TEXCOORD;

typedef struct {
    signed short coord[3];
    char normal[2];

    void dumpInfo() {
        printf("%d %d %d\n", coord[0], coord[1], coord[2]);
    }
} MD3_VERTEX;

template<typename T>
static void dp(const char* a, T& b)
{
    std::cout << a << ": " << b << std::endl;
}

static void dumpSurfaceInfo(MD3_SURFACE* s)
{
    using namespace std;
    cout << "Surface--" << endl;
    cout << "Ident: " << s->ident << endl;
    cout << "name: " << s->name << endl;
    cout << "flags " << s->flags << endl;
    dp("frames", s->num_frames);
    dp("shaders", s->num_shaders);
    dp("verts", s->num_verts);
    dp("triangles", s->num_triangles);
    cout << "ofs_end " << s->ofs_end << endl;
}

static void dumpShaderInfo(MD3_SHADER* s)
{
    using namespace std;
    cout << "-Shader " << s->shader_index << ": " << s->name << endl;
}

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

static void dumpTriangleInfo(MD3_TRIANGLE* t)
{
    printf("tri %d %d %d\n", t[0], t[1], t[2]);
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

    //~ dumpHeaderInfo(md3_header);

    int surf_offset = 0;
    for(int i = 0; i < md3_header->num_surfaces; ++i) {
        MD3_SURFACE* md3_surface =
            (MD3_SURFACE*) ((unsigned char*) mapbase + md3_header->ofs_surfaces + surf_offset);

        /*MD3_VERTEX* md3_vertex =
            (MD3_VERTEX*) md3_surface + md3_surface->ofs_xyznormal;
        dumpVertexInfo(md3_vertex);*/
        /*for(int j = 0; j < md3_surface->num_verts; ++j) {
            MD3_VERTEX* md3_vertex =
                (MD3_VERTEX*) md3_surface + md3_surface->ofs_xyznormal;

        }*/

        MD3_SHADER* md3_shaders =
            (MD3_SHADER*) ((uint8_t*)md3_surface + md3_surface->ofs_shaders);

        for(int j = 0; j < md3_surface->num_shaders; ++j) {
            //~ dumpShaderInfo(&md3_shaders[j]);
        }

        uint8_t* uffe = (uint8_t*)md3_surface;

        MD3_TRIANGLE* md3_triangles =
            (MD3_TRIANGLE*) (uffe + md3_surface->ofs_triangles);
        MD3_TEXCOORD* md3_texcoords =
            (MD3_TEXCOORD*) (uffe + md3_surface->ofs_st);
        MD3_VERTEX* md3_vertices =
            (MD3_VERTEX*) (uffe + md3_surface->ofs_xyznormal);

        for(int k = 0; k < md3_surface->num_triangles; ++k) {
            //~ dumpTriangleInfo(&md3_triangles[k]);
            //~ md3_texcoords[k].dumpInfo();
        }

        for(int k = 0; k < md3_surface->num_verts; ++k) {
            //~ dumpTriangleInfo(&md3_triangles[k]);
            md3_vertices[k].dumpInfo();
        }

        surf_offset += md3_surface->ofs_end;

    }

    free(mapbase);
    close(file_fd);
    return result;
}
