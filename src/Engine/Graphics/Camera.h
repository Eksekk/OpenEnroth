#pragma once

#include <array>

#include <glm.hpp>

#include "Engine/OurMath.h"

#include "Engine/Graphics/IRender.h"

// TODO(captainurist): this is actually Planef, but dot = -dist.
struct IndoorCameraD3D_Vec4 {
    float x = 0;
    float y = 0;
    float z = 0;
    float dot = 0;
};

struct Camera3D {
    void ViewTransform(int x, int y, int z, int *transformed_x, int *transformed_y, int *transformed_z);
    void ViewTransform(struct RenderVertexSoft *a1a, unsigned int uNumVertices);

    bool ViewClip(int x, int y, int z, int *transformed_x, int *transformed_y,
                  int *transformed_z, bool dont_show = false);

    void Project(int x, int y, int z, int *screenspace_x, int *screenspace_y);
    void Project(struct RenderVertexSoft *pVertices, unsigned int uNumVertices,
                 bool fit_into_viewport = false);

    bool CullFaceToCameraFrustum(RenderVertexSoft *pInVertices,
        unsigned int *pOutNumVertices,
        RenderVertexSoft *pVertices,
        signed int NumFrustumPlanes);

    float GetPolygonMaxZ(struct RenderVertexSoft *pVertex,
                          unsigned int uStripType);
    float GetPolygonMinZ(struct RenderVertexSoft *pVertices,
                          unsigned int uStripType);

    void LightmapNeerClip(struct RenderVertexSoft *pInVertices,
                          int uNumInVertices,
                          struct RenderVertexSoft *pOutVertices,
                          unsigned int *pOutNumVertices);
    void LightmapFarClip(struct RenderVertexSoft *pInVertices,
                         int uNumInVertices,
                         struct RenderVertexSoft *pOutVertices,
                         unsigned int *pOutNumVertices);
    void LightmapProject(unsigned int uNumInVertices,
                 struct RenderVertexSoft *pOutVertices,
                 struct RenderVertexSoft *pInVertices,
                 signed int *pOutNumVertices);
    bool CullFaceToFrustum(struct RenderVertexSoft *a1,
                         unsigned int *pOutNumVertices,
                         struct RenderVertexSoft *pVertices, signed int uNumVertices);

    bool CullFaceToFrustum(struct RenderVertexSoft *inVerts,
        unsigned int *pOutNumVertices,
        struct RenderVertexSoft *pOutVertices, struct IndoorCameraD3D_Vec4 *frustum, signed int uNumPlanes);

    bool ClipFaceToFrustum(RenderVertexSoft *pInVertices,
        unsigned int *pOutNumVertices,
        RenderVertexSoft *pVertices,
        IndoorCameraD3D_Vec4 *CameraFrustrum,
        signed int NumFrustumPlanes, char DebugLines,
        int _unused);

    void BuildViewFrustum();
    void CreateViewMatrixAndProjectionScale();

    void debug_outline_sw(struct RenderVertexSoft *a2,
                          unsigned int uNumVertices, unsigned int uDiffuse32,
                          float a5);

    void do_draw_debug_line_sw(struct RenderVertexSoft *pLineBegin,
                               signed int sStartDiffuse32,
                               struct RenderVertexSoft *pLineEnd,
                               signed int sEndDiffuse32,
                               unsigned int uOutNumVertices, float z_stuff);
    bool is_face_faced_to_cameraBLV(struct BLVFace *pFace);
    bool is_face_faced_to_cameraODM(struct ODMFace *pFace, struct RenderVertexSoft *a2);
    static void GetFacetOrientation(const Vec3f &normal, Vec3f *outU, Vec3f *outV);

    void CullByNearClip(struct RenderVertexSoft *pverts, uint *unumverts);
    void CullByFarClip(struct RenderVertexSoft *pverts, uint *unumverts);

    float GetMouseInfoDepth();

    glm::mat3x3 ViewMatrix = {};
    // using w comp of vec4 for dotdist
    std::array<glm::vec4, 6> FrustumPlanes = {{}};

    // unit fov is normalised focal ratio
    float unit_fov = 0;
    // field of view in vertical direction in degrees for GL
    float fov_y_deg = 0;
    // centre of the game viewport
    float screenCenterX = 0;
    float screenCenterY = 0;
    // these are the effective focal distances of the camera in x and y
    float ViewPlaneDist_X = 0;
    float ViewPlaneDist_Y = 0;

    // Camera field of view angles in degrees and radians
    int odm_fov_deg = 75;
    float odm_fov_rad = odm_fov_deg * pi / 180.0f;
    int blv_fov_deg = 60;
    float blv_fov_rad = blv_fov_deg * pi / 180.0f;

    // game viewport aspect ratio
    float aspect = 0;

    // camera cos + sin values in both forms to avoid repeated calculation
    void CalculateRotations(int cameraYaw, int cameraPitch);
    int _viewYaw = 0;
    int _viewPitch = 0;
    float _yawRotationSine = 0;
    float _yawRotationCosine = 0;
    float _pitchRotationSine = 0;
    float _pitchRotationCosine = 0;

    glm::vec3 vCameraPos = {};


    float GetNearClip() const;
    float GetFarClip() const;
};

extern Camera3D *pCamera3D;
