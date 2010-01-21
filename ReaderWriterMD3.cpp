#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osg/Geode>
#include <osg/Texture2D>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <iostream>

#define PI  (3.1415927f)
#define PI2 (2*PI)

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

template<typename T>
static void dp(const char* a, T& b)
{
    std::cout << a << ": " << b << std::endl;
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

    void dumpInfo() {
        printf("-Header-\n");
        dp("ident", ident);
        dp("version", version);
        dp("name", name);
        dp("flags", flags);
        dp("frames", num_frames);
        dp("tags", num_tags);
        dp("surfaces", num_surfaces);
        dp("skins", num_skins);
        dp("ofs_frames", ofs_frames);
        dp("ofs_tags", ofs_tags);
        dp("ofs_surfaces", ofs_surfaces);
        dp("ofs_eof", ofs_eof);
    }
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

    void dumpInfo() {
        std::cout << "tag " << name << std::endl;
        printf("origin %4.2f %4.2f %4.2f\n", origin.x, origin.y, origin.z);
    }
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

    void dumpInfo() {
        printf("Shader %d: %s\n", shader_index, name);
    }
} MD3_SHADER;

typedef struct {
    int indices[3];
} MD3_TRIANGLE;

typedef struct {
    float uv[2];

    float u() {
        return uv[0];
    }

    float v() {
        return 1.f - uv[1];
    }

    void dumpInfo() {
        std::cout << uv[0] << " " << uv[1] << std::endl;
    }
} MD3_TEXCOORD;

typedef struct {
    signed short coord[3];
    short normal;

    float posX() {
        return coord[0]/64.f;
    }

    float posY() {
        return coord[1]/64.f;
    }

    float posZ() {
        return coord[2]/64.f;
    }

    float lng() {
        return (normal & 0xFF) * PI2 / 255.0f;
    }

    float lat() {
        return ((normal >> 8) & 0xFF) * PI2 / 255.0f;
    }

    float normX() {
        return cosf(lat()) * sinf(lng());
    }

    float normY() {
        return sinf(lat()) * sinf(lng());
    }

    float normZ() {
        return cosf(lng());
    }

    void dumpInfo() {
        printf("%1.1f %1.1f %1.1f | %1.1f %1.1f | ",
               posX(), posY(), posZ(), lng(), lat());
        printf("%1.1f %1.1f %1.1f\n", normX(), normY(), normZ()); 
    }
} MD3_VERTEX;

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
    osg::Geode* result = 0;
    result = new osg::Geode;

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

    md3_header->dumpInfo();

    MD3_TAG* md3_tags =
        (MD3_TAG*) ((unsigned char*) mapbase + md3_header->ofs_tags);
    for(int i = 0; i < md3_header->num_tags; ++i) {
        md3_tags[i].dumpInfo();
    }

    int surf_offset = 0;
    for(int i = 0; i < md3_header->num_surfaces; ++i) {
        MD3_SURFACE* md3_surface =
            (MD3_SURFACE*) ((unsigned char*) mapbase + md3_header->ofs_surfaces + surf_offset);

        uint8_t* uffe = (uint8_t*)md3_surface;

        MD3_SHADER* md3_shaders =
            (MD3_SHADER*) (uffe + md3_surface->ofs_shaders);
        MD3_TRIANGLE* md3_triangles =
            (MD3_TRIANGLE*) (uffe + md3_surface->ofs_triangles);
        MD3_TEXCOORD* md3_texcoords =
            (MD3_TEXCOORD*) (uffe + md3_surface->ofs_st);
        MD3_VERTEX* md3_vertices =
            (MD3_VERTEX*) (uffe + md3_surface->ofs_xyznormal);

        std::vector<osg::Texture2D*> textures;

        osg::ref_ptr<osg::Texture2D> skin_texture = 0;
        for(int j = 0; j < md3_surface->num_shaders; ++j) {
            //~ md3_shaders[j].dumpInfo();
            osg::ref_ptr<osg::Image> img;
            osg::ref_ptr<osg::Texture2D> tex;
            std::string imgname(md3_shaders[j].name);
            img = osgDB::readRefImageFile(imgname, options);
            if(img.valid()) {
                tex = new osg::Texture2D;
                tex->setImage(img);
                skin_texture = tex.get();
                textures.push_back(tex);
                continue;
            }

            textures.push_back(0);
            osg::notify(osg::WARN) << "MD3 loader: couldn't load texture " << imgname << std::endl;
        }

        osg::UIntArray* vertexIndices = new osg::UIntArray;
        osg::UIntArray* normalIndices = new osg::UIntArray;
        osg::Vec2Array* texCoords = new osg::Vec2Array;
        for(int k = 0; k < md3_surface->num_triangles; ++k) {
            vertexIndices->push_back(md3_triangles[k].indices[0]);
            vertexIndices->push_back(md3_triangles[k].indices[1]);
            vertexIndices->push_back(md3_triangles[k].indices[2]);

            normalIndices->push_back(md3_triangles[k].indices[0]);
            normalIndices->push_back(md3_triangles[k].indices[1]);
            normalIndices->push_back(md3_triangles[k].indices[2]);

            //~ md3_texcoords[k].dumpInfo();
            texCoords->push_back(osg::Vec2(md3_texcoords[k].u(),
                                           md3_texcoords[k].v()));
        }

        osg::Geometry* geom = new osg::Geometry;
        osg::Vec3Array* vertexCoords = new osg::Vec3Array;
        osg::Vec3Array* normalCoords = new osg::Vec3Array;
        for(int k = 0; k < md3_surface->num_verts; ++k) {
            //~ md3_vertices[k].dumpInfo();
            vertexCoords->push_back(osg::Vec3(md3_vertices[k].posX(),
                                              md3_vertices[k].posY(),
                                              md3_vertices[k].posZ()));

            normalCoords->push_back(osg::Vec3(md3_vertices[k].normX(),
                                              md3_vertices[k].normY(),
                                              md3_vertices[k].normZ()));
        }

        geom->setVertexArray(vertexCoords);
        geom->setVertexIndices(vertexIndices);
        geom->setTexCoordArray(0, texCoords);
        geom->setTexCoordIndices(0, normalIndices);
        geom->setNormalArray(normalCoords);
        geom->setNormalIndices(normalIndices);
        geom->setNormalBinding (osg::Geometry::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES,
                              0, vertexIndices->size()));
        result->addDrawable(geom);

        osg::StateSet* state = new osg::StateSet;
        state->setTextureAttributeAndModes(0, skin_texture.get(), osg::StateAttribute::ON);
        geom->setStateSet(state);

        surf_offset += md3_surface->ofs_end;

    }

    free(mapbase);
    close(file_fd);
    return result;
}
