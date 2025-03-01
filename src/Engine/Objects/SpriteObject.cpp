#include "Engine/Objects/SpriteObject.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "Engine/Engine.h"
#include "Engine/SpellFxRenderer.h"
#include "Engine/Time.h"
#include "Engine/Events/Processor.h"
#include "Engine/LOD.h"
#include "Engine/OurMath.h"
#include "Engine/Party.h"
#include "Engine/TurnEngine/TurnEngine.h"
#include "Engine/AttackList.h"

#include "Engine/Objects/Actor.h"
#include "Engine/Objects/ObjectList.h"

#include "Engine/Tables/ItemTable.h"

#include "Engine/Graphics/Collisions.h"
#include "Engine/Graphics/BSPModel.h"
#include "Engine/Graphics/Level/Decoration.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/ParticleEngine.h"
#include "Engine/Graphics/Sprites.h"

#include "Media/Audio/AudioPlayer.h"

#include "Utility/Math/TrigLut.h"
#include "Library/Random/Random.h"

// should be injected in SpriteObject but struct size cant be changed
static SpellFxRenderer *spell_fx_renderer = EngineIocContainer::ResolveSpellFxRenderer();
static std::shared_ptr<ParticleEngine> particle_engine = EngineIocContainer::ResolveParticleEngine();

std::vector<SpriteObject> pSpriteObjects;

int SpriteObject::Create(int yaw, int pitch, int speed, int which_char) {
    // check for valid sprite object
    if (!uObjectDescID) {
        return -1;
    }

    // find free sprite slot
    int sprite_slot = -1;
    for (unsigned int i = 0; i < pSpriteObjects.size(); ++i) {
        if (!pSpriteObjects[i].uObjectDescID) {
            sprite_slot = i;
            break;
        }
    }

    if (sprite_slot == -1) {
        sprite_slot = pSpriteObjects.size();
        pSpriteObjects.emplace_back();
    }

    // set initial position
    initialPosition = vPosition;

    // set start timer for particle emmission
    _lastParticleTime = pEventTimer->uTotalGameTimeElapsed;

    // move sprite so it looks like it originates from char portrait
    switch (which_char) {
        case 0:
            break;  // do nothing
        case 1:
            Vec3i::rotate((24/*<<16*/), 2048 - uFacing, 0, vPosition, &vPosition.x, &vPosition.y, &vPosition.z);
            break;
        case 2:
            Vec3i::rotate((8/*<<16*/), 2048 - uFacing, 0, vPosition, &vPosition.x, &vPosition.y, &vPosition.z);
            break;
        case 3:
            Vec3i::rotate((8/*<<16*/), 1024 - uFacing, 0, vPosition, &vPosition.x, &vPosition.y, &vPosition.z);
            break;
        case 4:
            Vec3i::rotate((24/*<<16*/), 1024 - uFacing, 0, vPosition, &vPosition.x, &vPosition.y, &vPosition.z);
            break;
        default:
            assert(false);
            return 0;
            break;
    }

    // set blank velocity
    vVelocity = Vec3s(0, 0, 0);

    // calcualte angle velocity - could use rotate func here as above
    if (speed) {
        vVelocity.x = TrigLUT.cos(yaw) * TrigLUT.cos(pitch) * speed;
        vVelocity.y = TrigLUT.sin(yaw) * TrigLUT.cos(pitch) * speed;
        vVelocity.z = TrigLUT.sin(pitch) * speed;
    }

    // copy sprite object into slot
    if (sprite_slot >= (int)pSpriteObjects.size()) {
        pSpriteObjects.resize(sprite_slot + 1);
    }
    pSpriteObjects[sprite_slot] = *this;
    return sprite_slot;
}

static void createSpriteTrailParticle(Vec3i pos, OBJECT_DESC_FLAGS flags) {
    Particle_sw particle;
    memset(&particle, 0, sizeof(Particle_sw));
    particle.x = pos.x;
    particle.y = pos.y;
    particle.z = pos.z;
    if (flags & OBJECT_DESC_TRIAL_FIRE) {
        particle.type = ParticleType_Bitmap | ParticleType_Rotating | ParticleType_Ascending;
        particle.uDiffuse = colorTable.OrangeyRed.c32();
        particle.timeToLive = vrng->random(0x80) + 128; // was rand() & 0x80
        particle.texture = spell_fx_renderer->effpar01;
        particle.particle_size = 1.0f;
        particle_engine->AddParticle(&particle);
    } else if (flags & OBJECT_DESC_TRIAL_LINE) {
        particle.type = ParticleType_Line;
        particle.uDiffuse = vrng->random(RAND_MAX); // TODO(captainurist): TBH this makes no sense, investigate
        particle.timeToLive = 64;
        particle.texture = nullptr;
        particle.particle_size = 1.0f;
        particle_engine->AddParticle(&particle);
    } else if (flags & OBJECT_DESC_TRIAL_PARTICLE) {
        particle.type = ParticleType_Bitmap | ParticleType_Ascending;
        particle.uDiffuse = vrng->random(RAND_MAX);
        particle.timeToLive = vrng->random(0x80) + 128; // was rand() & 0x80
        particle.texture = spell_fx_renderer->effpar03;
        particle.particle_size = 1.0f;
        particle_engine->AddParticle(&particle);
    }
}

void SpriteObject::updateObjectODM(unsigned int uLayingItemID) {
    ObjectDesc *object = &pObjectList->pObjects[pSpriteObjects[uLayingItemID].uObjectDescID];
    bool isHighSlope = IsTerrainSlopeTooHigh(pSpriteObjects[uLayingItemID].vPosition.x, pSpriteObjects[uLayingItemID].vPosition.y);
    int bmodelPid = 0;
    bool onWater = false;
    int level = ODM_GetFloorLevel(pSpriteObjects[uLayingItemID].vPosition, object->uHeight, &onWater, &bmodelPid, 0);
    bool isAboveGround = pSpriteObjects[uLayingItemID].vPosition.z > level + 1;
    if (!isAboveGround && onWater) {
        int splashZ = level + 60;
        if (bmodelPid) {
            splashZ = level + 30;
        }
        createSplashObject({pSpriteObjects[uLayingItemID].vPosition.x, pSpriteObjects[uLayingItemID].vPosition.y, splashZ});
        SpriteObject::OnInteraction(uLayingItemID);
    }

    if (!(object->uFlags & OBJECT_DESC_NO_GRAVITY)) {
        if (isAboveGround) {
            pSpriteObjects[uLayingItemID].vVelocity.z -= pEventTimer->uTimeElapsed * GetGravityStrength();
        } else if (isHighSlope) {
            Vec3i norm;
            ODM_GetTerrainNormalAt(pSpriteObjects[uLayingItemID].vPosition.x, pSpriteObjects[uLayingItemID].vPosition.y, &norm);
            pSpriteObjects[uLayingItemID].vPosition.z = level + 1;
            pSpriteObjects[uLayingItemID].vVelocity.z -= (pEventTimer->uTimeElapsed * GetGravityStrength());

            int dotFix = abs(dot(norm, pSpriteObjects[uLayingItemID].vVelocity)) >> 16;
            // v60 = ((uint64_t)(v56 * (int64_t)v51.x) >> 16);
            // v60 = ((uint64_t)(v56 * (int64_t)v51.y) >> 16);
            // v60 = ((uint64_t)(v56 * (int64_t)v51.z) >> 16);
            pSpriteObjects[uLayingItemID].vVelocity.x += fixpoint_mul(dotFix, norm.x);
            pSpriteObjects[uLayingItemID].vVelocity.y += fixpoint_mul(dotFix, norm.y);
            pSpriteObjects[uLayingItemID].vVelocity.z += fixpoint_mul(dotFix, norm.z);
        } else {
            if (object->uFlags & OBJECT_DESC_INTERACTABLE) {
                if (pSpriteObjects[uLayingItemID].vPosition.z < level) {
                    pSpriteObjects[uLayingItemID].vPosition.z = level + 1;
                }
                if (!processSpellImpact(uLayingItemID, 0)) {
                    return;
                }
            }
            pSpriteObjects[uLayingItemID].vPosition.z = level + 1;
            if (object->uFlags & OBJECT_DESC_BOUNCE) {
                int bounceZVel = -(pSpriteObjects[uLayingItemID].vVelocity.z / 2);
                pSpriteObjects[uLayingItemID].vVelocity.z = bounceZVel;
                if (bounceZVel < 10) {
                    pSpriteObjects[uLayingItemID].vVelocity.z = 0;
                }
            } else {
                pSpriteObjects[uLayingItemID].vVelocity.z = 0;
            }

            pSpriteObjects[uLayingItemID].vVelocity.x = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.x);
            pSpriteObjects[uLayingItemID].vVelocity.y = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.y);
            pSpriteObjects[uLayingItemID].vVelocity.z = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.z);
            if (pSpriteObjects[uLayingItemID].vVelocity.getXY().lengthSqr() < 400) {
                pSpriteObjects[uLayingItemID].vVelocity.x = 0;
                pSpriteObjects[uLayingItemID].vVelocity.y = 0;
                createSpriteTrailParticle(pSpriteObjects[uLayingItemID].vPosition, object->uFlags);
                return;
            }
        }
    }
    if (object->uFlags & OBJECT_DESC_INTERACTABLE) {
        if (abs(pSpriteObjects[uLayingItemID].vPosition.x) > 32768 ||
                abs(pSpriteObjects[uLayingItemID].vPosition.y) > 32768 ||
                pSpriteObjects[uLayingItemID].vPosition.z <= level ||
                pSpriteObjects[uLayingItemID].vPosition.z > 13000) {
            if (pSpriteObjects[uLayingItemID].vPosition.z < level) {
                pSpriteObjects[uLayingItemID].vPosition.z = level + 1;
            }
            if (!processSpellImpact(uLayingItemID, 0)) {
                return;
            }
        }
    }

    collision_state.check_hi = false;
    collision_state.radius_lo = object->uRadius;
    collision_state.radius_hi = 0;
    collision_state.total_move_distance = 0;
    for (int i = 0; i < 100; i++) {
        collision_state.uSectorID = 0;
        collision_state.position_lo = pSpriteObjects[uLayingItemID].vPosition.toFloat() + Vec3f(0, 0, collision_state.radius_lo + 1);
        collision_state.position_hi = collision_state.position_lo;
        collision_state.velocity = pSpriteObjects[uLayingItemID].vVelocity.toFloat();
        if (collision_state.PrepareAndCheckIfStationary(0)) {
            return;
        }

        CollideOutdoorWithModels(false);
        int gridX = WorldPosToGridCellX(pSpriteObjects[uLayingItemID].vPosition.x);
        int gridY = WorldPosToGridCellY(pSpriteObjects[uLayingItemID].vPosition.y);
        CollideOutdoorWithDecorations(gridX, gridY);
        ObjectType casterType = PID_TYPE(pSpriteObjects[uLayingItemID].spell_caster_pid);
        if (casterType != OBJECT_Player) {
            CollideWithParty(false);
        }
        if (casterType == OBJECT_Actor) {
            int actorId = PID_ID(pSpriteObjects[uLayingItemID].spell_caster_pid);
            // TODO: why pActors.size() - 1? Should just check for .size()
            if ((actorId >= 0) && (actorId < (pActors.size() - 1))) {
                for (int j = 0; j < pActors.size(); ++j) {
                    if (pActors[actorId].GetActorsRelation(&pActors[j])) {
                        CollideWithActor(j, 0);
                    }
                }
            }
        } else {
            for (int j = 0; j < pActors.size(); ++j) {
                CollideWithActor(j, 0);
            }
        }
        int collisionZ = collision_state.new_position_lo.z - collision_state.radius_lo - 1;
        bool collisionOnWater = false;
        int collisionBmodelPid = 0;
        Vec3i collisionPos = collision_state.new_position_lo.toInt() - Vec3i(0, 0, collision_state.radius_lo + 1);
        int collisionLevel = ODM_GetFloorLevel(collisionPos, object->uHeight, &collisionOnWater, &collisionBmodelPid, 0);
        // TOOD(Nik-RE-dev): why initail "onWater" is used?
        if (onWater && collisionZ < (collisionLevel + 60)) {
            int splashZ = level + 60;
            if (collisionBmodelPid) {
                splashZ = collisionLevel + 30;
            }
            createSplashObject({pSpriteObjects[uLayingItemID].vPosition.x, pSpriteObjects[uLayingItemID].vPosition.y, splashZ});
            SpriteObject::OnInteraction(uLayingItemID);
            return;
        }
        if (collision_state.adjusted_move_distance >= collision_state.move_distance) {
            pSpriteObjects[uLayingItemID].vPosition = (collision_state.new_position_lo - Vec3f(0, 0, collision_state.radius_lo + 1)).toIntTrunc();
            //pSpriteObjects[uLayingItemID].vPosition.x = collision_state.new_position_lo.x;
            //pSpriteObjects[uLayingItemID].vPosition.y = collision_state.new_position_lo.y;
            //pSpriteObjects[uLayingItemID].vPosition.z = collision_state.new_position_lo.z - collision_state.radius_lo - 1;
            pSpriteObjects[uLayingItemID].uSectorID = collision_state.uSectorID;
            createSpriteTrailParticle(pSpriteObjects[uLayingItemID].vPosition, object->uFlags);
            return;
        }
        // v60 = ((uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.x) >> 16);
        // v60 = ((uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.y) >> 16);
        // v60 = ((uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.z) >> 16);
        Vec3f delta = collision_state.direction * collision_state.adjusted_move_distance;
        pSpriteObjects[uLayingItemID].vPosition += delta.toInt();
        pSpriteObjects[uLayingItemID].uSectorID = collision_state.uSectorID;
        collision_state.total_move_distance += collision_state.adjusted_move_distance;
        if (object->uFlags & OBJECT_DESC_INTERACTABLE) {
            if (pSpriteObjects[uLayingItemID].vPosition.z < level) {
                pSpriteObjects[uLayingItemID].vPosition.z = level + 1;
            }
            if (!processSpellImpact(uLayingItemID, collision_state.pid)) {
                return;
            }
        }
        if (PID_TYPE(collision_state.pid) == OBJECT_Decoration) {
            break;
        }
        if (PID_TYPE(collision_state.pid) == OBJECT_Face) {
            BSPModel *bmodel = &pOutdoor->pBModels[(signed int)collision_state.pid >> 9];
            ODMFace *face = &bmodel->pFaces[PID_ID(collision_state.pid) & 0x3F];
            if (face->uPolygonType == POLYGON_Floor) {
                pSpriteObjects[uLayingItemID].vPosition.z = bmodel->pVertices[face->pVertexIDs[0]].z + 1;
                if (pSpriteObjects[uLayingItemID].vVelocity.getXY().lengthSqr() >= 400) {
                    if (face->uAttributes & FACE_TriggerByObject) {
                        eventProcessor(face->sCogTriggeredID, 0, 1);
                    }
                } else {
                    pSpriteObjects[uLayingItemID].vVelocity = Vec3s(0, 0, 0);
                }
            } else {
                float dotFix = abs(dot(face->facePlane.normal, pSpriteObjects[uLayingItemID].vVelocity.toFloat()));
                dotFix = std::max(dotFix, collision_state.speed / 8);
                float newZVel = dotFix * face->facePlane.normal.z;
                pSpriteObjects[uLayingItemID].vVelocity.x += 2 * dotFix * face->facePlane.normal.x;
                pSpriteObjects[uLayingItemID].vVelocity.y += 2 * dotFix * face->facePlane.normal.y;
                if (face->facePlane.normal.z <= 0.48828125f) { // was 32000 fixpoint, 32000/65536=0.488...
                    newZVel = 2 * newZVel;
                } else {
                    pSpriteObjects[uLayingItemID].vVelocity.z += newZVel;
                    newZVel = 0.48828125f * newZVel;
                }
                pSpriteObjects[uLayingItemID].vVelocity.z += newZVel;
                if (face->uAttributes & FACE_TriggerByObject) {
                    eventProcessor(face->sCogTriggeredID, 0, 1);
                }
            }
        }
        //LABEL_74:
        pSpriteObjects[uLayingItemID].vVelocity.x = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.x);
        pSpriteObjects[uLayingItemID].vVelocity.y = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.y);
        pSpriteObjects[uLayingItemID].vVelocity.z = fixpoint_mul(58500, pSpriteObjects[uLayingItemID].vVelocity.z);
    }
    Vec2i deltaXY = pSpriteObjects[uLayingItemID].vPosition.getXY() - pLevelDecorations[PID_ID(collision_state.pid)].vPosition.getXY();
    int velLenXY = integer_sqrt(pSpriteObjects[uLayingItemID].vVelocity.getXY().lengthSqr());
    int velRotXY = TrigLUT.atan2(deltaXY.x, deltaXY.y);

    pSpriteObjects[uLayingItemID].vVelocity.x = TrigLUT.cos(velRotXY) * velLenXY;
    pSpriteObjects[uLayingItemID].vVelocity.y = TrigLUT.sin(velRotXY - TrigLUT.uIntegerHalfPi) * velLenXY;
    //goto LABEL_74; // This goto results in an infinite loop, commented out.
}

//----- (0047136C) --------------------------------------------------------
void SpriteObject::updateObjectBLV(unsigned int uLayingItemID) {
    SpriteObject *pSpriteObject = &pSpriteObjects[uLayingItemID];
    ObjectDesc *pObject = &pObjectList->pObjects[pSpriteObject->uObjectDescID];

    // Break early if we're out of bounds.
    if (abs(pSpriteObject->vPosition.x) > 32767 ||
            abs(pSpriteObject->vPosition.y) > 32767 ||
            abs(pSpriteObject->vPosition.z) > 20000) {
        SpriteObject::OnInteraction(uLayingItemID);
        return;
    }

    unsigned int uFaceID;
    unsigned int uSectorID = pSpriteObject->uSectorID; // TODO: this should go straight into GetIndoorFloorZ as a pointer.
    int floor_lvl = GetIndoorFloorZ(pSpriteObject->vPosition, &uSectorID, &uFaceID);
    if (floor_lvl <= -30000) {
        SpriteObject::OnInteraction(uLayingItemID);
        return;
    }

    pSpriteObject->uSectorID = uSectorID;

    if (pObject->uFlags & OBJECT_DESC_NO_GRAVITY) {
        goto LABEL_25;
    }

    // flying objects / projectiles
    if (floor_lvl <= pSpriteObject->vPosition.z - 3) {
        pSpriteObject->vVelocity.z -= pEventTimer->uTimeElapsed * GetGravityStrength();
        // TODO(Nik-RE-dev): get rid of goto here
LABEL_25:
        collision_state.check_hi = false;
        collision_state.radius_lo = pObject->uRadius;
        collision_state.ignored_face_id = -1;
        collision_state.radius_hi = 0;
        collision_state.total_move_distance = 0;
        for (int loop = 0; loop < 100; loop++) {
            collision_state.position_hi = pSpriteObject->vPosition.toFloat() + Vec3f(0, 0, collision_state.radius_lo + 1);
            collision_state.position_lo = collision_state.position_hi;
            collision_state.velocity = pSpriteObject->vVelocity.toFloat();
            collision_state.uSectorID = pSpriteObject->uSectorID;
            if (collision_state.PrepareAndCheckIfStationary(0)) {
                return;
            }

            // TODO(Nik-RE-dev): check purpose of inner loop
            for (int loop2 = 0; loop2 < 100; ++loop2) {
                CollideIndoorWithGeometry(false);
                CollideIndoorWithDecorations();

                if (PID_TYPE(pSpriteObject->spell_caster_pid) != OBJECT_Player) {
                    CollideWithParty(true);
                }

                for (int actloop = 0; actloop < (signed int)pActors.size(); ++actloop) {
                    // dont collide against self monster type
                    if (PID_TYPE(pSpriteObject->spell_caster_pid) == OBJECT_Actor) {
                        if (pActors[PID_ID(pSpriteObject->spell_caster_pid)].pMonsterInfo.uID == pActors[actloop].pMonsterInfo.uID) {
                            continue;
                        }
                    }

                    // not sure:
                    // pMonsterList->pMonsters[v39b->word_000086_some_monster_id-1].uToHitRadius
                    int radius = 0;
                    if (pActors[actloop].word_000086_some_monster_id) {  // not always filled in from scripted monsters
                        radius = pMonsterList->pMonsters[pActors[actloop].word_000086_some_monster_id - 1].uToHitRadius;
                    }
                    CollideWithActor(actloop, radius);
                }

                if (CollideIndoorWithPortals()) {
                    break;
                }
            }
            // end loop2

            if (collision_state.adjusted_move_distance >= collision_state.move_distance) {
                pSpriteObject->vPosition = (collision_state.new_position_lo - Vec3f(0, 0, collision_state.radius_lo + 1)).toIntTrunc();
                pSpriteObject->uSectorID = collision_state.uSectorID;
                if (!(pObject->uFlags & OBJECT_DESC_TRIAL_PARTICLE)) {
                    return;
                }
                createSpriteTrailParticle(pSpriteObject->vPosition, pObject->uFlags);
                return;
            }
            // v40 = (uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.x) >> 16;
            // v40 = (uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.y) >> 16;
            // v40 = (uint64_t)(collision_state.adjusted_move_distance * (signed int64_t)collision_state.direction.z) >> 16;

            Vec3f delta = collision_state.direction * collision_state.adjusted_move_distance;
            pSpriteObject->vPosition += delta.toInt();
            pSpriteObject->uSectorID = collision_state.uSectorID;
            collision_state.total_move_distance += collision_state.adjusted_move_distance;

            // if weve collided but dont need to react return
            if ((pObject->uFlags & OBJECT_DESC_INTERACTABLE) &&
                !processSpellImpact(uLayingItemID, collision_state.pid)) {
                return;
            }

            int pidId = PID_ID(collision_state.pid);
            if (PID_TYPE(collision_state.pid) == OBJECT_Decoration) {
                Vec2i deltaXY = pSpriteObject->vPosition.getXY() - pLevelDecorations[pidId].vPosition.getXY();
                int velXYLen = integer_sqrt(pSpriteObject->vVelocity.getXY().lengthSqr());
                int velXYRot = TrigLUT.atan2(deltaXY.x, deltaXY.y);
                pSpriteObject->vVelocity.x = TrigLUT.cos(velXYRot) * velXYLen;
                pSpriteObject->vVelocity.y = TrigLUT.sin(velXYRot) * velXYLen;
            }
            if (PID_TYPE(collision_state.pid) == OBJECT_Face) {
                collision_state.ignored_face_id = PID_ID(collision_state.pid);
                if (pIndoor->pFaces[pidId].uPolygonType != POLYGON_Floor) {
                    // Before this variable changed floor_lvl variable which is obviously invalid.
                    float dotFix = abs(dot(pIndoor->pFaces[pidId].facePlane.normal, pSpriteObject->vVelocity.toFloat()));
                    dotFix = std::max(dotFix, collision_state.speed / 8);
                    pSpriteObject->vVelocity.x += 2 * dotFix * pIndoor->pFaces[pidId].facePlane.normal.x;
                    pSpriteObject->vVelocity.y += 2 * dotFix * pIndoor->pFaces[pidId].facePlane.normal.y;
                    float newZVel = dotFix * pIndoor->pFaces[pidId].facePlane.normal.z;
                    if (pIndoor->pFaces[pidId].facePlane.normal.z <= 0.48828125f) { // was 32000 fixpoint
                        newZVel = 2 * newZVel;
                    } else {
                        pSpriteObject->vVelocity.z += newZVel;
                        newZVel = 0.48828125f * newZVel;
                    }
                    pSpriteObject->vVelocity.z += newZVel;
                    if (pIndoor->pFaces[pidId].uAttributes & FACE_TriggerByObject) {
                        eventProcessor(pIndoor->pFaceExtras[pIndoor->pFaces[pidId].uFaceExtraID].uEventID, 0, 1);
                    }
                    pSpriteObject->vVelocity.x = fixpoint_mul(58500, pSpriteObject->vVelocity.x);
                    pSpriteObject->vVelocity.y = fixpoint_mul(58500, pSpriteObject->vVelocity.y);
                    pSpriteObject->vVelocity.z = fixpoint_mul(58500, pSpriteObject->vVelocity.z);
                    continue;
                }
                if (pObject->uFlags & OBJECT_DESC_BOUNCE) {
                    pSpriteObject->vVelocity.z = -pSpriteObject->vVelocity.z / 2;
                    if (pSpriteObject->vVelocity.z < 10) {
                        pSpriteObject->vVelocity.z = 0;
                    }
                    if (pIndoor->pFaces[pidId].uAttributes & FACE_TriggerByObject) {
                        eventProcessor(pIndoor->pFaceExtras[pIndoor->pFaces[pidId].uFaceExtraID].uEventID, 0, 1);
                    }
                    pSpriteObject->vVelocity.x = fixpoint_mul(58500, pSpriteObject->vVelocity.x);
                    pSpriteObject->vVelocity.y = fixpoint_mul(58500, pSpriteObject->vVelocity.y);
                    pSpriteObject->vVelocity.z = fixpoint_mul(58500, pSpriteObject->vVelocity.z);
                    continue;
                }
                pSpriteObject->vVelocity.z = 0;
                if (pSpriteObject->vVelocity.getXY().lengthSqr() >= 400) {
                    if (pIndoor->pFaces[pidId].uAttributes & FACE_TriggerByObject) {
                        eventProcessor(pIndoor->pFaceExtras[pIndoor->pFaces[pidId].uFaceExtraID].uEventID, 0, 1);
                    }
                    pSpriteObject->vVelocity.x = fixpoint_mul(58500, pSpriteObject->vVelocity.x);
                    pSpriteObject->vVelocity.y = fixpoint_mul(58500, pSpriteObject->vVelocity.y);
                    pSpriteObject->vVelocity.z = fixpoint_mul(58500, pSpriteObject->vVelocity.z);
                    continue;
                }
                pSpriteObject->vVelocity = Vec3s(0, 0, 0);
                pSpriteObject->vPosition.z = pIndoor->pVertices[*pIndoor->pFaces[pidId].pVertexIDs].z + 1;
            }
            pSpriteObject->vVelocity.x = fixpoint_mul(58500, pSpriteObject->vVelocity.x);
            pSpriteObject->vVelocity.y = fixpoint_mul(58500, pSpriteObject->vVelocity.y);
            pSpriteObject->vVelocity.z = fixpoint_mul(58500, pSpriteObject->vVelocity.z);
        }
        // end loop
    }

    if (!(pObject->uFlags & OBJECT_DESC_INTERACTABLE) || processSpellImpact(uLayingItemID, 0)) {
        pSpriteObject->vPosition.z = floor_lvl + 1;
        if (pIndoor->pFaces[uFaceID].uPolygonType == POLYGON_Floor) {
            pSpriteObject->vVelocity.z = 0;
        } else {
            if (pIndoor->pFaces[uFaceID].facePlane.normal.z < 0.68664550781f) { // was 45000 fixpoint
                pSpriteObject->vVelocity.z -= pEventTimer->uTimeElapsed * GetGravityStrength();
            }
        }
        pSpriteObject->vVelocity.x = fixpoint_mul(58500, pSpriteObject->vVelocity.x);
        pSpriteObject->vVelocity.y = fixpoint_mul(58500, pSpriteObject->vVelocity.y);
        pSpriteObject->vVelocity.z = fixpoint_mul(58500, pSpriteObject->vVelocity.z);
        if (pSpriteObject->vVelocity.getXY().lengthSqr() < 400) {
            pSpriteObject->vVelocity = Vec3s(0, 0, 0);
            if (!(pObject->uFlags & OBJECT_DESC_NO_SPRITE)) {
                return;
            }
            createSpriteTrailParticle(pSpriteObject->vPosition, pObject->uFlags);
            return;
        }
        // TODO(Nik-RE-dev): is this correct?
        goto LABEL_25;
    }
}

void SpriteObject::explosionTraps() {
    MapInfo *pMapInfo = &pMapStats->pInfos[pMapStats->GetMapInfo(pCurrentMapName)];
    int dir_x = abs(pParty->vPosition.x - this->vPosition.x);
    int dir_y = abs(pParty->vPosition.y - this->vPosition.y);
    int dir_z = abs(pParty->vPosition.z + pParty->sEyelevel - this->vPosition.z);
    if (dir_x < dir_y) {
        std::swap(dir_x, dir_y);
    }
    if (dir_x < dir_z) {
        std::swap(dir_x, dir_z);
    }
    if (dir_y < dir_z) {
        std::swap(dir_y, dir_z);
    }
    unsigned int v10 = ((unsigned int)(11 * dir_y) >> 5) + (dir_z / 4) + dir_x;
    if (v10 <= 768) {
        int trapDamage = 5;
        if (pMapInfo->Trap_D20) {
            trapDamage += grng->randomDice(pMapInfo->Trap_D20, 20);
        }
        DAMAGE_TYPE pDamageType;
        switch (this->uType) {
            case SPRITE_TRAP_FIRE:
                pDamageType = DMGT_FIRE;
                break;
            case SPRITE_TRAP_LIGHTNING:
                pDamageType = DMGT_ELECTR;
                break;
            case SPRITE_TRAP_COLD:
                pDamageType = DMGT_COLD;
                break;
            case SPRITE_TRAP_BODY:
                pDamageType = DMGT_BODY;
                break;
            default:
                return;
        }
        for (Player &player : pParty->pPlayers) {
            int perceptionCheckValue = player.GetPerception() + 20;
            if (player.CanAct() && (grng->random(perceptionCheckValue) > 20)) {
                player.playReaction(SPEECH_AvoidDamage);
            } else {
                player.receiveDamage(trapDamage, pDamageType);
            }
        }
    }
}

unsigned int SpriteObject::GetLifetime() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return pObjectDesc->uLifetime;
}

SpriteFrame *SpriteObject::getSpriteFrame() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return pSpriteFrameTable->GetFrame(pObjectDesc->uSpriteID, uSpriteFrameID);
}

bool SpriteObject::IsUnpickable() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return ((pObjectDesc->uFlags & OBJECT_DESC_UNPICKABLE) == OBJECT_DESC_UNPICKABLE);
}

bool SpriteObject::HasSprite() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return !pObjectDesc->NoSprite();
}

uint8_t SpriteObject::GetParticleTrailColorR() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return pObjectDesc->uParticleTrailColorR;
}

uint8_t SpriteObject::GetParticleTrailColorG() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return pObjectDesc->uParticleTrailColorG;
}

uint8_t SpriteObject::GetParticleTrailColorB() {
    ObjectDesc *pObjectDesc = &pObjectList->pObjects[uObjectDescID];
    return pObjectDesc->uParticleTrailColorB;
}

void SpriteObject::OnInteraction(unsigned int uLayingItemID) {
    pSpriteObjects[uLayingItemID].uObjectDescID = 0;
    if (pParty->bTurnBasedModeOn) {
        if (pSpriteObjects[uLayingItemID].uAttributes & SPRITE_HALT_TURN_BASED) {
            pSpriteObjects[uLayingItemID].uAttributes &= ~SPRITE_HALT_TURN_BASED;
            --pTurnEngine->pending_actions;
        }
    }
}

void CompactLayingItemsList() {
    int new_obj_pos = 0;

    for (int i = 0; i < pSpriteObjects.size(); ++i) {
        if (pSpriteObjects[i].uObjectDescID) {
            if (i != new_obj_pos) {
                pSpriteObjects[new_obj_pos] = pSpriteObjects[i];
                pSpriteObjects[i].uObjectDescID = 0;
            }
            new_obj_pos++;
        }
    }

    pSpriteObjects.resize(new_obj_pos);
}

void SpriteObject::InitializeSpriteObjects() {
    for (size_t i = 0; i < pSpriteObjects.size(); ++i) {
        SpriteObject *item = &pSpriteObjects[i];
        if (item->uObjectDescID && (item->uSoundID & 8 || pObjectList->pObjects[item->uObjectDescID].uFlags & OBJECT_DESC_UNPICKABLE)) {
            SpriteObject::OnInteraction(i);
        }
    }
}

// Process GM Shrink Ray spell that affect group of actors
bool SpriteObject::applyShrinkRayAoe() {
    bool isApplied = false;
    // Calculation was moved from initial sprite creation processing
    GameTime duration = GameTime::FromMinutes(this->spell_level * 5);
    static const int shrinkPower = 4;
    int effectDistance = engine->config->gameplay.ShrinkRayAoeDistance.value();

    for (Actor &actor : pActors) {
        // TODO(Nik-RE-dev): paralyzed actor will not be affected?
        if (actor.CanAct()) {
            int distanceSq = (actor.vPosition - this->vPosition + Vec3i(0, 0, actor.uActorHeight / 2)).lengthSqr();
            int checkDistanceSq = (effectDistance + actor.uActorRadius) * (effectDistance + actor.uActorRadius);

            if (distanceSq <= checkDistanceSq) {
                if (actor.DoesDmgTypeDoDamage(DMGT_DARK)) {
                    actor.pActorBuffs[ACTOR_BUFF_SHRINK].Apply(pParty->GetPlayingTime() + duration, this->spell_skill, shrinkPower, 0, 0);
                    actor.uAttributes |= ACTOR_AGGRESSOR;
                    isApplied = true;
                }
            }
        }
    }
    return isApplied;
}

bool SpriteObject::dropItemAt(SPRITE_OBJECT_TYPE sprite, Vec3i pos, int speed, int count,
                              bool randomRotate, SPRITE_ATTRIBUTES attributes, ItemGen *item) {
    SpriteObject pSpellObject;

    pSpellObject.uType = sprite;
    pSpellObject.uObjectDescID = pObjectList->ObjectIDByItemID(sprite);
    pSpellObject.vPosition = pos;
    pSpellObject.uAttributes = attributes;
    pSpellObject.uSectorID = pIndoor->GetSector(pos);
    pSpellObject.containing_item.Reset();
    if (item) {
        pSpellObject.containing_item = *item;
    }

    if (!(pSpellObject.uAttributes & SPRITE_IGNORE_RANGE)) {
        for (ITEM_TYPE i : pItemTable->pItems.indices()) {
            if (pItemTable->pItems[i].uSpriteID == sprite) {
                pSpellObject.containing_item.uItemID = i;
            }
        }
    }

    if (randomRotate) {
        for (int i = 0; i < count; i++) {
            // Not sure if using grng is right here, but would rather err on the side of safety.
            pSpellObject.uFacing = grng->random(TrigLUT.uIntegerDoublePi);
            int pitch = TrigLUT.uIntegerQuarterPi + grng->random(TrigLUT.uIntegerQuarterPi);
            pSpellObject.Create(pSpellObject.uFacing, pitch, speed, 0);
        }
    } else {
        pSpellObject.uFacing = 0;
        for (int i = 0; i < count; i++) {
            pSpellObject.Create(pSpellObject.uFacing, TrigLUT.uIntegerHalfPi, speed, 0);
        }
    }
    return true;
}

// splash on water
void SpriteObject::createSplashObject(Vec3i pos) {
    SpriteObject sprite;
    sprite.containing_item.Reset();
    sprite.uType = SPRITE_WATER_SPLASH;
    sprite.uObjectDescID = pObjectList->ObjectIDByItemID(sprite.uType);
    sprite.vPosition = pos;
    sprite.uSectorID = pIndoor->GetSector(pos);
    int objID = sprite.Create(0, 0, 0, 0);
    if (objID != -1) {
        pAudioPlayer->playSound(SOUND_splash, PID(OBJECT_Item, objID), 0, 0, 0);
    }
}

static void updateSpriteOnImpact(SpriteObject *object) {
    object->uType = static_cast<SPRITE_OBJECT_TYPE>(object->uType + 1);
    object->uObjectDescID = pObjectList->ObjectIDByItemID(object->uType);
}

bool processSpellImpact(unsigned int uLayingItemID, int pid) {
    SpriteObject *object = &pSpriteObjects[uLayingItemID];
    ObjectDesc *objectDesc = &pObjectList->pObjects[object->uObjectDescID];

    if (PID_TYPE(pid) == OBJECT_Actor) {
        if (PID_TYPE(object->spell_caster_pid) == OBJECT_Actor && !pActors[PID_ID(object->spell_caster_pid)].GetActorsRelation(&pActors[PID_ID(pid)])) {
            return 1;
        }
    } else {
        if (PID_TYPE(pid) == OBJECT_Player && PID_TYPE(object->spell_caster_pid) == OBJECT_Player) {
            return 1;
        }
    }

    if (pParty->bTurnBasedModeOn) {
        if (object->uAttributes & SPRITE_HALT_TURN_BASED) {
            --pTurnEngine->pending_actions;
            object->uAttributes &= ~SPRITE_HALT_TURN_BASED;
        }
    }
    if (PID_TYPE(pid) == OBJECT_Face && PID_TYPE(object->spell_caster_pid) != OBJECT_Player) {
        if (PID_ID(object->spell_caster_pid) < 500) {  // bugfix  PID_ID(v2->spell_caster_pid)==1000
            pActors[PID_ID(object->spell_caster_pid)].uAttributes |= ACTOR_UNKNOW5;
        }
    }

    switch (object->uType) {
        case SPRITE_SPELL_FIRE_FIRE_SPIKE:
        case SPRITE_SPELL_AIR_SPARKS:
        case SPRITE_SPELL_DARK_TOXIC_CLOUD: {
            if (PID_TYPE(pid) == OBJECT_Face
                || PID_TYPE(pid) == OBJECT_Decoration
                || PID_TYPE(pid) == OBJECT_None) {
                return 1;
            }
            if (PID_TYPE(pid) != OBJECT_Item) {
                applySpellSpriteDamage(uLayingItemID, pid);
                updateSpriteOnImpact(object);
                if (object->uObjectDescID == 0) {
                    SpriteObject::OnInteraction(uLayingItemID);
                }
                object->spellSpriteStop();
                // v97 = 0;
                // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
                //     v97 = (int16_t)pSpriteObjects[uLayingItemID].uSoundID + 4;
                // }
                //            v125 =
                //            word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
                //            - 1] + 1; pAudioPlayer->playSound((SoundID)v125,
                //            v124, 0, -1, 0, v97, 0, 0);
                pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
                return 0;
            }
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            pAudioPlayer->playSound(SOUND_fireBall, PID(OBJECT_Item, uLayingItemID));
            return 0;
        }

        case SPRITE_PROJECTILE_AIRBOLT:
        case SPRITE_PROJECTILE_EARTHBOLT:
        case SPRITE_PROJECTILE_FIREBOLT:
        case SPRITE_PROJECTILE_WATERBOLT:
        case SPRITE_PROJECTILE_520:
        case SPRITE_PROJECTILE_525:
        case SPRITE_PROJECTILE_530:
        case SPRITE_PROJECTILE_LIGHTBOLT:
        case SPRITE_PROJECTILE_DARKBOLT: {
            applySpellSpriteDamage(uLayingItemID, pid);
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            if (object->uType == SPRITE_BLASTER_PROJECTILE) {
                // TODO(Nik-RE-dev): unreachable, these cases does not process this sprite type
                pAudioPlayer->playSound(SOUND_fireBall, PID(OBJECT_Item, uLayingItemID));
            }
            return 0;
        }

        case SPRITE_ARROW_PROJECTILE:
        case SPRITE_PROJECTILE_EXPLOSIVE: {
            // Note that ITEM_SPELLBOOK_FIREBALL is an MM6 remnant here,
            // in MM6 it was Percival artifact (id 405) which has swiftness and carnage enchantments
            if (object->containing_item.uItemID != ITEM_SPELLBOOK_FIREBALL &&
                object->containing_item.special_enchantment != ITEM_ENCHANTMENT_OF_CARNAGE) {
                object->spellSpriteStop();
                applySpellSpriteDamage(uLayingItemID, pid);
                SpriteObject::OnInteraction(uLayingItemID);
                // v16 = 0;
                // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
                //     v16 = pSpriteObjects[uLayingItemID].uSoundID + 4;
                // }
                //            v124 = 8 * uLayingItemID;
                //            v124 |= 2;
                //            v125 =
                //            word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id]
                //            + 1; pAudioPlayer->playSound((SoundID)v125, v124,
                //            0, -1, 0, v16, 0, 0);
                pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
                return 0;
            }
            object->uType = SPRITE_OBJECT_EXPLODE;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_OBJECT_EXPLODE);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            object->uObjectDescID = pObjectList->ObjectIDByItemID(object->uType);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            if (object->uType != SPRITE_BLASTER_PROJECTILE) {
                pAudioPlayer->playSound(SOUND_fireBall, PID(OBJECT_Item, uLayingItemID));
                return 0;
            }
            return 0;
        }

        case SPRITE_OBJECT_EXPLODE: {  // actor death explode
            object->uType = SPRITE_OBJECT_EXPLODE_IMPACT;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_OBJECT_EXPLODE_IMPACT);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            pushAoeAttack(PID(OBJECT_Item, uLayingItemID), engine->config->gameplay.AoeDamageDistance.value(),
                    pSpriteObjects[uLayingItemID].vPosition, ABILITY_ATTACK1);
            if (objectDesc->uFlags & OBJECT_DESC_TRIAL_PARTICLE) {
                trail_particle_generator.GenerateTrailParticles(object->vPosition.x, object->vPosition.y, object->vPosition.z,
                                                                objectDesc->uParticleTrailColor);
            }
            pAudioPlayer->playSound(SOUND_fireBall, PID(OBJECT_Item, uLayingItemID));
            return 0;
        }

        case SPRITE_SPELL_FIRE_FIRE_BOLT:
        case SPRITE_SPELL_FIRE_INCINERATE:
        case SPRITE_SPELL_AIR_LIGHTNING_BOLT:
        case SPRITE_SPELL_WATER_POISON_SPRAY:
        case SPRITE_SPELL_WATER_ICE_BOLT:
        case SPRITE_SPELL_WATER_ACID_BURST:
        case SPRITE_SPELL_EARTH_STUN:
        case SPRITE_SPELL_EARTH_DEADLY_SWARM:
        case SPRITE_SPELL_EARTH_BLADES:
        case SPRITE_SPELL_EARTH_MASS_DISTORTION:
        case SPRITE_SPELL_MIND_MIND_BLAST:
        case SPRITE_SPELL_MIND_PSYCHIC_SHOCK:
        case SPRITE_SPELL_BODY_HARM:
        case SPRITE_SPELL_BODY_FLYING_FIST:
        case SPRITE_SPELL_LIGHT_LIGHT_BOLT:
        case SPRITE_SPELL_LIGHT_SUNRAY:
        case SPRITE_SPELL_DARK_SHARPMETAL: {
            applySpellSpriteDamage(uLayingItemID, pid);
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            // int v97 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v97 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v124 = 8 * uLayingItemID;
            //        v124 |= v124 | 2;
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v124, 0,
            //        -1, 0, v97, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_BLASTER_PROJECTILE: {
            applySpellSpriteDamage(uLayingItemID, pid);
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            return 0;
        }

        case SPRITE_SPELL_WATER_ICE_BLAST: {
            object->uType = SPRITE_SPELL_WATER_ICE_BLAST_FALLOUT;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_SPELL_WATER_ICE_BLAST_FALLOUT);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->vVelocity = Vec3s(0.0, 0.0, 0.0);
            int iceParticles = (object->spell_skill == PLAYER_SKILL_MASTERY_GRANDMASTER) ? 9 : 7;
            int yaw = object->uFacing - TrigLUT.uIntegerDoublePi;
            for (int i = 0; i < iceParticles; i++) {
                yaw += TrigLUT.uIntegerQuarterPi;
                object->Create(yaw, 0, 1000, 0);
            }
            SpriteObject::OnInteraction(uLayingItemID);
            // int v16 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v16 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v124 = 8 * uLayingItemID;
            //        v124 |= v124 | 2;
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v124, 0,
            //        -1, 0, v16, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_WATER_ICE_BLAST_FALLOUT: {
            object->uType = SPRITE_SPELL_WATER_ICE_BLAST_IMPACT;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_SPELL_WATER_ICE_BLAST_IMPACT);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            applySpellSpriteDamage(uLayingItemID, pid);
            // int v16 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v16 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v124 = 8 * uLayingItemID;
            //        v124 |= v124 | 2;
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v124, 0,
            //        -1, 0, v16, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_EARTH_ROCK_BLAST: {
            if (PID_TYPE(pid) == OBJECT_Face || PID_TYPE(pid) == OBJECT_Decoration || PID_TYPE(pid) == OBJECT_None) {
                return 1;
            }
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            pushAoeAttack(PID(OBJECT_Item, uLayingItemID), engine->config->gameplay.AoeDamageDistance.value(),
                    pSpriteObjects[uLayingItemID].vPosition, ABILITY_ATTACK1);
            // int v78 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v78 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125,
            //        pSpriteObjects[uLayingItemID].vPosition.x, 0, -1, 0, v78,
            //        0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_EARTH_DEATH_BLOSSOM: {
            object->uType = SPRITE_SPELL_EARTH_DEATH_BLOSSOM_FALLOUT;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_SPELL_EARTH_DEATH_BLOSSOM_FALLOUT);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->vVelocity = Vec3s(0.0, 0.0, 0.0);
            int yaw = object->uFacing - TrigLUT.uIntegerDoublePi;
            for (int i = 0; i < 8; i++) {
                int yawRandomDelta = grng->randomInSegment(-128, 128);
                int randomSpeed = grng->randomInSegment(5, 500);
                yaw += TrigLUT.uIntegerQuarterPi;
                object->Create(yaw + yawRandomDelta, 0, randomSpeed, 0);
            }
            SpriteObject::OnInteraction(uLayingItemID);
            // int v16 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v16 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v124 = 8 * uLayingItemID;
            //        v124 |= 2;
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v124, 0,
            //        -1, 0, v16, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_EARTH_DEATH_BLOSSOM_FALLOUT: {
            object->uType = SPRITE_SPELL_EARTH_DEATH_BLOSSOM_IMPACT;
            object->uObjectDescID = pObjectList->ObjectIDByItemID(SPRITE_SPELL_EARTH_DEATH_BLOSSOM_IMPACT);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            pushAoeAttack(PID(OBJECT_Item, uLayingItemID), engine->config->gameplay.AoeDamageDistance.value(),
                    pSpriteObjects[uLayingItemID].vPosition, object->field_61);
            // int v78 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v78 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125,
            //        pSpriteObjects[uLayingItemID].vPosition.x, 0, -1, 0, v78,
            //        0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_LIGHT_DESTROY_UNDEAD: {
            if (PID_TYPE(pid) == OBJECT_Actor &&
                MonsterStats::BelongsToSupertype(pActors[PID_ID(pid)].pMonsterInfo.uID, MONSTER_SUPERTYPE_UNDEAD)) {
                applySpellSpriteDamage(uLayingItemID, pid);
            }
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            // int v97 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v97 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v92 = uLayingItemID;
            //        v124 = 8 * v92;
            //        v124 |= 2;
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v124, 0,
            //        -1, 0, v97, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        case SPRITE_SPELL_MIND_CHARM:
        case SPRITE_SPELL_LIGHT_PARALYZE:
        case SPRITE_SPELL_DARK_SHRINKING_RAY:  {
            // int v143 = 17030;
            // switch (pSpriteObjects[uLayingItemID].uType) {
            //     case SPRITE_SPELL_MIND_CHARM:
            //         v143 = 15040;
            //         break;
            //     case SPRITE_SPELL_EARTH_SLOW:
            //         v143 = 13010;
            //         break;
            //     case SPRITE_SPELL_DARK_SHRINKING_RAY:
            //         v143 = 18030;
            //         break;
            //     default:
            //         break;
            // }
            bool isDamaged = false;
            bool isShrinkingRayAoe = (object->uType == SPRITE_SPELL_DARK_SHRINKING_RAY) && (object->spell_skill == PLAYER_SKILL_MASTERY_GRANDMASTER);
            if (PID_TYPE(pid) != OBJECT_Actor) {
                if (!isShrinkingRayAoe) {
                    SpriteObject::OnInteraction(uLayingItemID);
                    return 0;
                }
                isDamaged = object->applyShrinkRayAoe();
                if (isDamaged) {
                    updateSpriteOnImpact(object);
                    if (object->uObjectDescID == 0) {
                        SpriteObject::OnInteraction(uLayingItemID);
                    }
                    object->spellSpriteStop();
                    // int v114 = 0;
                    // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
                    //     v114 = pSpriteObjects[uLayingItemID].uSoundID + 4;
                    // }
                    //                v115 = 8 * uLayingItemID;
                    //                v115 |= PID(OBJECT_Item, uLayingItemID);
                    //                v125 = v143 + 1;
                    //                pAudioPlayer->playSound((SoundID)v125,
                    //                v115, 0, -1, 0, v114, 0, 0);
                    pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
                } else {
                    SpriteObject::OnInteraction(uLayingItemID);
                }
                return 0;
            }
            int shrinkPower = 0;
            // Calculation was moved from initial sprite creation processing
            GameTime duration = GameTime::FromMinutes(object->spell_level * 5);
            PLAYER_SKILL_MASTERY skillMastery = object->spell_skill;
            DAMAGE_TYPE dmgType;
            ACTOR_BUFF_INDEX buffIdx;
            switch (object->uType) {
                case SPRITE_SPELL_MIND_CHARM:
                    dmgType = DMGT_MIND;
                    buffIdx = ACTOR_BUFF_CHARM;
                    break;
                case SPRITE_SPELL_LIGHT_PARALYZE:
                    dmgType = DMGT_LIGHT;
                    buffIdx = ACTOR_BUFF_PARALYZED;
                    break;
                case SPRITE_SPELL_DARK_SHRINKING_RAY:
                    dmgType = DMGT_DARK;
                    buffIdx = ACTOR_BUFF_SHRINK;
                    break;
            }
            if (object->uType == SPRITE_SPELL_DARK_SHRINKING_RAY) {
                switch (skillMastery) {
                    case PLAYER_SKILL_MASTERY_NOVICE:
                        shrinkPower = 2;
                        break;
                    case PLAYER_SKILL_MASTERY_EXPERT:
                        shrinkPower = 3;
                        break;
                    case PLAYER_SKILL_MASTERY_MASTER:
                    case PLAYER_SKILL_MASTERY_GRANDMASTER:
                        shrinkPower = 4;
                        break;
                }
                pActors[PID_ID(pid)].uAttributes |= ACTOR_AGGRESSOR;
            }

            if (!isShrinkingRayAoe) {
                int actorId = PID_ID(pid);
                if (pActors[PID_ID(pid)].DoesDmgTypeDoDamage(dmgType)) {
                    isDamaged = true;
                    if (object->uType == SPRITE_SPELL_LIGHT_PARALYZE) {
                        pActors[actorId].uAIState = Standing;
                        pActors[actorId].UpdateAnimation();
                    }
                    pActors[actorId].pActorBuffs[buffIdx].Apply(pParty->GetPlayingTime() + duration, skillMastery, shrinkPower, 0, 0);
                }
            } else {
                isDamaged = object->applyShrinkRayAoe();
            }
            object->spell_level = 0;
            object->spell_skill = PLAYER_SKILL_MASTERY_NONE;
            object->uSpellID = SPELL_NONE;
            if (isDamaged) {
                updateSpriteOnImpact(object);
                if (object->uObjectDescID == 0) {
                    SpriteObject::OnInteraction(uLayingItemID);
                }
                object->spellSpriteStop();
                // int v114 = 0;
                // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
                //     v114 = pSpriteObjects[uLayingItemID].uSoundID + 4;
                // }
                //            v115 = 8 * uLayingItemID;
                //            v115 |= PID(OBJECT_Item, uLayingItemID);
                //            v125 = v143 + 1;
                //            pAudioPlayer->playSound((SoundID)v125, v115, 0,
                //            -1, 0, v114, 0, 0);
                pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            } else {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            return 0;
        }

            /*
            case 1080:
            case 2100:
            {
            if (PID_TYPE(a2) != 3)
            {
            //v32 = 0;
            pSpriteObjects[uLayingItemID].uType =
            pSpriteObjects[uLayingItemID].uType + 1; v46 = 0; for (v146 = 0;
            v146 < (signed int)pObjectList->uNumObjects; ++v146)
            {
            if (pSpriteObjects[uLayingItemID].uType ==
            pObjectList->pObjects[v146].uObjectID) v46 = v146;
            }
            pSpriteObjects[uLayingItemID].uObjectDescID = v46;
            if (!v46)
            SpriteObject::OnInteraction(uLayingItemID);
            v100 = pSpriteObjects[uLayingItemID].field_61;
            pSpriteObjects[uLayingItemID].uSpriteFrameID = 0;
            v102 = 8 * uLayingItemID;
            LOBYTE(v102) = PID(OBJECT_Item, uLayingItemID);
            pSpriteObjects[uLayingItemID].vVelocity.x = 0;
            pSpriteObjects[uLayingItemID].vVelocity.y = 0;
            pSpriteObjects[uLayingItemID].vVelocity.z = 0;
            AttackerInfo.Add(v102, 512,
            pSpriteObjects[uLayingItemID].vPosition.x,
            pSpriteObjects[uLayingItemID].vPosition.y,
            pSpriteObjects[uLayingItemID].vPosition.z, v100, 0); if
            (objectDesc->uFlags & OBJECT_DESC_TRIAL_PARTICLE)
            trail_particle_generator.GenerateTrailParticles(pSpriteObjects[uLayingItemID].vPosition.x,
            pSpriteObjects[uLayingItemID].vPosition.y,
            pSpriteObjects[uLayingItemID].vPosition.z,
            objectDesc->uParticleTrailColor); if
            (!pSpriteObjects[uLayingItemID].uSoundID) v47 = 0; else v47 =
            (int16_t)pSpriteObjects[uLayingItemID].uSoundID + 4; v125 =
            word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id - 1] +
            1; pAudioPlayer->playSound((SoundID)v125, v102, 0, -1, 0, v47, 0,
            0); return 0;
            }
            return 1;
            }*/

        case SPRITE_SPELL_FIRE_METEOR_SHOWER:
        case SPRITE_SPELL_AIR_STARBURST: {
            if (PID_TYPE(pid) == OBJECT_Actor) {
                return 1;
            }
            // else go to next case
        }

        case SPRITE_SPELL_FIRE_FIREBALL:
        case SPRITE_SPELL_DARK_DRAGON_BREATH: {
            updateSpriteOnImpact(object);
            if (object->uObjectDescID == 0) {
                SpriteObject::OnInteraction(uLayingItemID);
            }
            object->spellSpriteStop();
            pushAoeAttack(PID(OBJECT_Item, uLayingItemID), engine->config->gameplay.AoeDamageDistance.value(),
                    pSpriteObjects[uLayingItemID].vPosition, object->field_61);
            if (objectDesc->uFlags & OBJECT_DESC_TRIAL_PARTICLE) {
                trail_particle_generator.GenerateTrailParticles(
                    object->vPosition.x, object->vPosition.y, object->vPosition.z,
                    objectDesc->uParticleTrailColor);
            }
            // int v47 = 0;
            // if (pSpriteObjects[uLayingItemID].uSoundID != 0) {
            //     v47 = pSpriteObjects[uLayingItemID].uSoundID + 4;
            // }
            //        v125 =
            //        word_4EE088_sound_ids[pSpriteObjects[uLayingItemID].spell_id
            //        - 1] + 1; pAudioPlayer->playSound((SoundID)v125, v102, 0,
            //        -1, 0, v47, 0, 0);
            pAudioPlayer->playSpellSound(object->uSpellID, PID(OBJECT_Item, uLayingItemID), true);
            return 0;
        }

        default:
            return 0;
    }
}

void applySpellSpriteDamage(unsigned int uLayingItemID, int pid) {
    Vec3i velocity;

    if (PID_TYPE(pid) == OBJECT_Player) {
        velocity = pSpriteObjects[uLayingItemID].vVelocity;
        normalize_to_fixpoint(&velocity.x, &velocity.y, &velocity.z);
        DamagePlayerFromMonster(PID(OBJECT_Item, uLayingItemID), pSpriteObjects[uLayingItemID].field_61, &velocity, -1);
    } else if (PID_TYPE(pid) == OBJECT_Actor) {
        velocity = pSpriteObjects[uLayingItemID].vVelocity;
        normalize_to_fixpoint(&velocity.x, &velocity.y, &velocity.z);
        switch (PID_TYPE(pSpriteObjects[uLayingItemID].spell_caster_pid)) {
            case OBJECT_Actor:
                Actor::ActorDamageFromMonster(PID(OBJECT_Item, uLayingItemID), PID_ID(pid), &velocity, pSpriteObjects[uLayingItemID].field_61);
                break;
            case OBJECT_Player:
                Actor::DamageMonsterFromParty(PID(OBJECT_Item, uLayingItemID), PID_ID(pid), &velocity);
                break;
            case OBJECT_Item:
                ItemDamageFromActor(PID(OBJECT_Item, uLayingItemID), PID_ID(pid), &velocity);
                break;
            default:
                break;
        }
    }
}

void UpdateObjects() {
    for (uint i = 0; i < pSpriteObjects.size(); ++i) {
        if (pSpriteObjects[i].uAttributes & SPRITE_SKIP_A_FRAME) {
            pSpriteObjects[i].uAttributes &= ~SPRITE_SKIP_A_FRAME;
        } else {
            ObjectDesc *object = &pObjectList->pObjects[pSpriteObjects[i].uObjectDescID];
            if (pSpriteObjects[i].attachedToActor()) {
                int actorId = PID_ID(pSpriteObjects[i].spell_target_pid);
                if (actorId > pActors.size()) {
                    continue;
                }
                pSpriteObjects[i].vPosition = pActors[actorId].vPosition + Vec3i(0, 0, pActors[actorId].uActorHeight);
                if (!pSpriteObjects[i].uObjectDescID) {
                    continue;
                }
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY)) {
                    continue;
                }
                if (pSpriteObjects[i].uSpriteFrameID >= 0) {
                    int lifetime = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & SPRITE_TEMPORARY) {
                        lifetime = pSpriteObjects[i].tempLifetime;
                    }
                    if (pSpriteObjects[i].uSpriteFrameID < lifetime) {
                        continue;
                    }
                }
                SpriteObject::OnInteraction(i);
                continue;
            }
            if (pSpriteObjects[i].uObjectDescID) {
                int lifetime = 0;
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (object->uFlags & OBJECT_DESC_TEMPORARY) {
                    if (pSpriteObjects[i].uSpriteFrameID < 0) {
                        SpriteObject::OnInteraction(i);
                        continue;
                    }
                    lifetime = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & SPRITE_TEMPORARY) {
                        lifetime = pSpriteObjects[i].tempLifetime;
                    }
                }
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY) ||
                    pSpriteObjects[i].uSpriteFrameID < lifetime) {
                    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
                        SpriteObject::updateObjectBLV(i);
                    } else {
                        SpriteObject::updateObjectODM(i);
                    }
                    if (!pParty->bTurnBasedModeOn || !(pSpriteObjects[i].uSectorID & 4)) {
                        continue;
                    }
                    if ((pParty->vPosition - pSpriteObjects[i].vPosition).length() <= 5120) {
                        continue;
                    }
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                if (!(object->uFlags & OBJECT_DESC_INTERACTABLE)) {
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                processSpellImpact(i, PID(OBJECT_Item, i));
            }
        }
    }
}

unsigned int collideWithActor(unsigned int uLayingItemID, signed int pid) {
    unsigned int result = uLayingItemID;
    if (pObjectList->pObjects[pSpriteObjects[uLayingItemID].uObjectDescID].uFlags & OBJECT_DESC_UNPICKABLE) {
        result = processSpellImpact(uLayingItemID, pid);
    }
    return result;
}
