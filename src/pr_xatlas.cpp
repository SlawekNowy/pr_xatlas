#include "pr_xatlas.hpp"
#include <xatlas.h>
#include <pragma/game/game.h>
#include <pragma/model/model.h>
#include <pragma/model/modelmesh.h>
#include <pragma/pragma_module.hpp>
#include <sharedutils/util_parallel_job.hpp>
#include <luainterface.hpp>

#pragma optimize("",off)
//extern DLLENGINE Engine *engine;
namespace Lua::xatlas
{
	void register_lua_library(Lua::Interface &l);
};

struct AtlasVertex
{
	uint16_t originalVertexIndex;
	Vector2 uv;
};
struct AtlasMesh
{
	std::vector<uint16_t> indices;
	std::vector<AtlasVertex> vertices;
};

class Atlas
	//: public util::ParallelWorker<std::vector<Vector2>&>
{
public:
	static std::shared_ptr<Atlas> Create();
	~Atlas();
	void AddMesh(const ModelSubMesh &mesh,const Material &material);

	//virtual std::vector<Vector2> &GetResult() override {return m_lightmapUvs;}
	std::vector<AtlasMesh> Generate();
private:
	Atlas(xatlas::Atlas *atlas);
	xatlas::Atlas *m_atlas = nullptr;
	std::unordered_map<const Material*,uint32_t> m_materialToIndex {};
	uint32_t m_materialIndex = 0;
	//std::vector<Vector2> m_lightmapUvs = {};
};

std::shared_ptr<Atlas> Atlas::Create()
{
	xatlas::Atlas *atlas = xatlas::Create();
	return std::shared_ptr<Atlas>{new Atlas{atlas}};
}

std::vector<AtlasMesh> Atlas::Generate()
{
	printf("Generating atlas...\n");
	xatlas::ChartOptions chartOptions {};
	chartOptions.maxChartArea = 0.0;
	chartOptions.maxBoundaryLength = 0.0;
	chartOptions.normalDeviationWeight = 2.0;
	chartOptions.roundnessWeight = 0.01;
	chartOptions.straightnessWeight = 6.0;
	chartOptions.normalSeamWeight = 4.0;
	chartOptions.textureSeamWeight = 0.5;
	chartOptions.maxCost = 2.0;
	chartOptions.maxIterations = 1;
	chartOptions.useInputMeshUvs = false;
	chartOptions.fixWinding = false;

	xatlas::PackOptions packOptions {};
	packOptions.maxChartSize = 0;
	packOptions.blockAlign = false;
	packOptions.texelsPerUnit = 0.0;
	packOptions.resolution = 0;
	packOptions.bilinear = true;
	packOptions.createImage = false;
	packOptions.bruteForce = false;
	packOptions.rotateChartsToAxis = true;
	packOptions.rotateCharts = true;
	packOptions.padding = 1;
	xatlas::Generate(m_atlas,chartOptions,packOptions);

	printf("   %d charts\n", m_atlas->chartCount);
	printf("   %d atlases\n", m_atlas->atlasCount);
	for (uint32_t i = 0; i < m_atlas->atlasCount; i++)
		printf("      %d: %0.2f%% utilization\n", i, m_atlas->utilization[i] * 100.0f);
	printf("   %ux%u resolution\n", m_atlas->width, m_atlas->height);
	//printf("%.2f seconds (%g ms) elapsed total\n", globalStopwatch.elapsed() / 1000.0, globalStopwatch.elapsed());
	std::cout<<"Done"<<std::endl;



	std::vector<AtlasMesh> atlasMeshes {};
	atlasMeshes.resize(m_atlas->meshCount);
	for(auto i=decltype(m_atlas->meshCount){0u};i<m_atlas->meshCount;++i)
	{
		auto &inMesh = m_atlas->meshes[i];
		auto &outMesh = atlasMeshes.at(i);
		auto &outVerts = outMesh.vertices;
		outMesh.indices.reserve(inMesh.indexCount);
		for(auto j=decltype(inMesh.indexCount){0u};j<inMesh.indexCount;++j)
			outMesh.indices.push_back(inMesh.indexArray[j]);

		outVerts.resize(inMesh.vertexCount);
		for(auto j=decltype(inMesh.vertexCount){0u};j<inMesh.vertexCount;++j)
		{
			auto &vIn = inMesh.vertexArray[j];
			auto &vOut = outVerts.at(j);
			vOut.uv = {vIn.uv[0] /static_cast<float>(m_atlas->width),vIn.uv[1] /static_cast<float>(m_atlas->height)};
			vOut.originalVertexIndex = vIn.xref;
		}
	}
	return atlasMeshes;
}

void Atlas::AddMesh(const ModelSubMesh &mesh,const Material &material)
{
	auto &verts = mesh.GetVertices();
	auto &tris = mesh.GetTriangles();

	uint32_t materialIndex = 0;
	auto it = m_materialToIndex.find(&material);
	if(it != m_materialToIndex.end())
		materialIndex = it->second;
	else
		m_materialToIndex.insert(std::make_pair(&material,m_materialIndex++));
	std::vector<uint32_t> materials;
	materials.resize(tris.size() /3,materialIndex);
	
	xatlas::MeshDecl meshDecl;
	auto *vertexData = reinterpret_cast<const uint8_t*>(verts.data());
	meshDecl.faceMaterialData = materials.data();
	meshDecl.vertexCount = verts.size();
	meshDecl.vertexPositionData = vertexData +offsetof(Vertex,position);
	meshDecl.vertexPositionStride = sizeof(Vertex);
	meshDecl.vertexNormalData = vertexData +offsetof(Vertex,normal);
	meshDecl.vertexNormalStride = sizeof(Vertex);
	meshDecl.vertexUvData = vertexData +offsetof(Vertex,uv);
	meshDecl.vertexUvStride = sizeof(Vertex);
	meshDecl.indexCount = tris.size();
	meshDecl.indexData = tris.data();
	meshDecl.indexFormat = xatlas::IndexFormat::UInt16;
	xatlas::AddMeshError error = xatlas::AddMesh(m_atlas,meshDecl);
	if(error != xatlas::AddMeshError::Success)
	{
		std::cout<<"Error: "<<xatlas::StringForEnum(error)<<std::endl;
		return;
	}
	
	xatlas::UvMeshDecl uvMeshDecl {};

	xatlas::AddMeshError error = xatlas::AddUvMesh(m_atlas,meshDecl);
#if 0
struct UvMeshDecl
{
	const void *vertexUvData = nullptr;
	const void *indexData = nullptr; // optional
	const uint32_t *faceMaterialData = nullptr; // Optional. Overlapping UVs should be assigned a different material. Must be indexCount / 3 in length.
	uint32_t vertexCount = 0;
	uint32_t vertexStride = 0;
	uint32_t indexCount = 0;
	int32_t indexOffset = 0; // optional. Add this offset to all indices.
	IndexFormat indexFormat = IndexFormat::UInt16;
};
	xatlas::AddMeshError error = xatlas::AddUvMesh(m_atlas,meshDecl);

#endif
}

Atlas::Atlas(xatlas::Atlas *atlas)
	: m_atlas{atlas}
{
	//AddThread([this]() {
	//	Generate();
	//});
}

Atlas::~Atlas()
{
	xatlas::Destroy(m_atlas);
}

static int Print(const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	printf("\r"); // Clear progress text.
	const int result = vprintf(format, arg);
	va_end(arg);
	return result;
}

static bool s_verbose = true;
static bool ProgressCallback(xatlas::ProgressCategory category, int progress, void *userData)
{
	// Don't interupt verbose printing.
	if (s_verbose)
		return true;
	/*Stopwatch *stopwatch = (Stopwatch *)userData;
	static std::mutex progressMutex;
	std::unique_lock<std::mutex> lock(progressMutex);
	if (progress == 0)
		stopwatch->reset();
	printf("\r   %s [", xatlas::StringForEnum(category));
	for (int i = 0; i < 10; i++)
		printf(progress / ((i + 1) * 10) ? "*" : " ");
	printf("] %d%%", progress);
	fflush(stdout);
	if (progress == 100)
		printf("\n      %.2f seconds (%g ms) elapsed\n", stopwatch->elapsed() / 1000.0, stopwatch->elapsed());*/
	return true;
}


extern "C"
{
	void PRAGMA_EXPORT pragma_initialize_lua(Lua::Interface &l)
	{
		if(l.GetIdentifier() == "gui")
			return;
		Lua::xatlas::register_lua_library(l);
	}
};

void Lua::xatlas::register_lua_library(Lua::Interface &l)
{
	auto xatlasMod = luabind::module(l.GetState(),"xatlas");
	xatlasMod[
		luabind::def("create",static_cast<std::shared_ptr<Atlas>(*)(lua_State*)>(
			[](lua_State *l) -> std::shared_ptr<Atlas> {
				auto atlas = Atlas::Create();
				return atlas;
		}))
	];

	auto defVertex = luabind::class_<AtlasVertex>("MeshVertex");
	defVertex.def_readwrite("uv",&AtlasVertex::uv);
	defVertex.def_readwrite("originalVertexIndex",&AtlasVertex::originalVertexIndex);
	xatlasMod[defVertex];
#if 0
struct AtlasMesh
{
	std::vector<uint16_t> indices;
	std::vector<AtlasVertex> vertices;
};
#endif
	auto defMesh = luabind::class_<AtlasMesh>("Mesh");
	defMesh.def("GetIndexCount",static_cast<uint32_t(*)(const AtlasMesh&)>([](const AtlasMesh &mesh) -> uint32_t {
		return mesh.indices.size();
	}));
	defMesh.def("GetVertexCount",static_cast<uint32_t(*)(const AtlasMesh&)>([](const AtlasMesh &mesh) -> uint32_t {
		return mesh.vertices.size();
	}));
	defMesh.def("GetVertex",static_cast<const AtlasVertex&(*)(const AtlasMesh&,uint32_t)>([](const AtlasMesh &mesh,uint32_t i) -> const AtlasVertex& {
		return mesh.vertices.at(i);
	}));
	defMesh.def("GetIndex",static_cast<uint16_t(*)(const AtlasMesh&,uint32_t)>([](const AtlasMesh &mesh,uint32_t i) -> uint16_t {
		return mesh.indices.at(i);
	}));
	xatlasMod[defMesh];

	auto defNode = luabind::class_<Atlas>("Atlas");
	defNode.def("AddMesh",static_cast<void(*)(Atlas&,const ModelSubMesh&,const Material&)>([](Atlas &atlas,const ModelSubMesh &mesh,const Material &mat) {
		atlas.AddMesh(mesh,mat);
	}));
	defNode.def("Generate",static_cast<luabind::object(*)(lua_State*,Atlas&)>([](lua_State *l,Atlas &atlas) -> luabind::object {
		auto meshes = atlas.Generate();
		return Lua::vector_to_table(l,meshes);
	}));
	xatlasMod[defNode];
}
#pragma optimize("",on)
