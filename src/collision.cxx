/*
 *   Copyright (c) 2019, Red                                                             *
 *   All rights reserved.                                                                *
 *                                                                                       *
 *   Redistribution and use in source and binary forms, with or without                  *
 *   modification, are permitted provided that the following conditions are met:         *
 *                                                                                       *
 *       * Redistributions of source code must retain the above copyright                *
 *         notice, this list of conditions and the following disclaimer.                 *
 *       * Redistributions in binary form must reproduce the above copyright             *
 *         notice, this list of conditions and the following disclaimer in the           *
 *         documentation and/or other materials provided with the distribution.          *
 *       * Neither the name of the modconv 2 developers nor the                          *
 *         names of its contributors may be used to endorse or promote products          *
 *         derived from this software without specific prior written permission.         *
 *                                                                                       *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND     *
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED       *
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE              *
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY                *
 *   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES          *
 *   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;        *
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND         *
 *   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS       *
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                        *
 */

/* This file handles creating collision data out of a model imported with ASSIMP. */
#include "modconv.hxx"

/* Globals */
u32 vertex = 0, internalVtx = 0;

typedef struct {
    s16 pos[3];
    u8 list;
    u16 material;
    bool useless;
} CollisionVtx;

typedef struct {
    u16 tri;
    std::string surf;
} CollisionMat;

static void configure_materials(const aiScene* scene, CollisionMat* mat)
{
    aiString aiName;
    u16 pos[2] = { 0 };
    for (u16 i = 0; i < scene->mNumMaterials; i++) {
        std::cout << i << " loops" << std::endl;
        scene->mMaterials[i]->Get(AI_MATKEY_NAME, aiName);
        mat[i].tri = 0;

        std::string nameStr = aiName.data;

        enum SurfaceString { STARTPOS, ENDPOS };

        /*
         * Find surface setting:
         * This searches for the first instance of !,
         * which will mark the beginning of the the surface
         * set, the ending is either the end of the string
         * or until the first instance of a space.
         */

        if (nameStr.find("!") != std::string::npos) {
            pos[STARTPOS] = nameStr.find("!") + 1;
            for (u16 j = 0; j < nameStr.length(); j++) {
                pos[ENDPOS] = j;
                if (nameStr[i] == ' ') {
                    pos[ENDPOS] = j - 1;
                    break;
                }
                mat[i].surf = nameStr.substr(pos[STARTPOS], pos[ENDPOS]);
            }
        } else {
            mat[i].surf = "SURF_ENV_DEFAULT";
        }
        std::cout << "Material " << aiName.data << " -> " << mat[i].surf << std::endl;
    }
}

static void inspect_vtx(aiNode* node, const aiScene* scene, CollisionMat* mat)
{
    for (u16 i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        vertex += mesh->mNumFaces * 3;
        mat[mesh->mMaterialIndex].tri += mesh->mNumFaces;
    }

    for (u16 i = 0; i < node->mNumChildren; i++) {
        inspect_vtx(node->mChildren[i], scene, mat);
    }
}

/* FBX multiplies vertex positions by 100. We counter this by multiplying FBX models by 0.01. */
static inline f32 scaling_hack(const std::string &file)
{
    if (file.substr(file.length() - 4, file.length()).compare(".fbx") == 0) {
        return 0.01f;
    } else {
        return 1.0f;
    }
}

static void setup_vtx(const std::string &file, aiNode* node, const aiScene* scene, CollisionVtx* vtx, const s16 scale)
{
    for (u16 i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        /* We go by faces, so we don't add loose geometry to our output. */
        for (u32 j = 0; j < mesh->mNumFaces; j++) {
            for (u8 k = 0; k < 3; k++) {
                u32 currVtx = mesh->mFaces[j].mIndices[k];
                vtx[internalVtx].pos[AXIS_X] = (s16)(((mesh->mVertices[currVtx].x) * scale) * scaling_hack(file));
                vtx[internalVtx].pos[AXIS_Y] = (s16)(((mesh->mVertices[currVtx].y) * scale) * scaling_hack(file));
                vtx[internalVtx].pos[AXIS_Z] = (s16)(((mesh->mVertices[currVtx].z) * scale) * scaling_hack(file));

                vtx[internalVtx].useless = false;
                vtx[internalVtx].list = internalVtx;
                vtx[internalVtx].material = mesh->mMaterialIndex;
                internalVtx++;
            }
        }
    }

    for (u16 i = 0; i < node->mNumChildren; i++) {
        setup_vtx(file, node->mChildren[i], scene, vtx, scale);
    }
}

static inline bool cprVtx(CollisionVtx* vtx, u32 i, u32 j)
{
    return (vtx[i].pos[AXIS_X] == vtx[j].pos[AXIS_X] &&
            vtx[i].pos[AXIS_Y] == vtx[j].pos[AXIS_Y] &&
            vtx[i].pos[AXIS_Z] == vtx[j].pos[AXIS_Z]);

}

static void clean_vtx(CollisionVtx* vtx)
{
    u32 writeSize = 0;
    /* Stage 1 - Mark redundant vertices */
    for (u32 i = 0; i < vertex; i++) {
        for (u32 j = 0; j < vertex; j++) {
            if (cprVtx(vtx, i, j) && j > i && !vtx[j].useless) {
                vtx[j].useless = true;
                vtx[j].list = i;
            }
        }
    }

    /* Stage 2 - Give verts that will be written their correct index. */
    for (u32 i = 0; i < vertex; i++) {
        if (!vtx[i].useless) {
            vtx[i].list = writeSize;
            writeSize++;
        }
    }
}

static void write_vtx(const std::string &fileOut, CollisionVtx* vtx)
{
    std::fstream colOut;
    colOut.open(fileOut + "/collision.s", std::iostream::out | std::iostream::app);
    for (u32 i = 0; i < vertex; i++) {
        if (vtx[i].useless == false) {
            std::cout << "vertex" << std::endl;
            colOut << "colVertex " << vtx[i].pos[AXIS_X] << ", "
                                   << vtx[i].pos[AXIS_Y] << ", "
                                   << vtx[i].pos[AXIS_Z] << std::endl;

        }
    }
}

/*static inline void get_vtx_index(const CollisionVtx* vtx)
{
}*/

/*static void write_tri(const std::string &fileOut, const CollisionVtx* vtx);
{
}*/

void collision_converter_main(const std::string &file, const std::string &fileOut, s16 scale)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(file, aiProcess_ValidateDataStructure | aiProcess_Triangulate | aiProcess_PreTransformVertices);

    std::fstream collisionOut;
    collisionOut.open(fileOut + "/collision.s", std::iostream::out | std::iostream::app);
    reset_file(fileOut + "/collision.s");
    collisionOut << "glabel " << get_filename(fileOut) << "_collision"
                 << std::endl << "colInit" << std::endl;
    collisionOut.close();

    CollisionMat mat[scene->mNumMaterials];
    std::cout << scene->mNumMaterials << std::endl;
    configure_materials(scene, mat);

    /* Count vtx amount, setup vtx and cleanup output */
    inspect_vtx(scene->mRootNode, scene, mat);
    CollisionVtx vtx[vertex];
    setup_vtx(file, scene->mRootNode, scene, vtx, scale);
    clean_vtx(vtx);

    /* Write data*/
    write_vtx(fileOut, vtx);
}
