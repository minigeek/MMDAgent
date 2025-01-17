/* ----------------------------------------------------------------- */
/*           The Toolkit for Building Voice Interaction Systems      */
/*           "MMDAgent" developed by MMDAgent Project Team           */
/*           http://www.mmdagent.jp/                                 */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2009-2011  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAgent project team nor the names of  */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

/* headers */

#include "MMDFiles.h"

/* PMDModel::resetBone: reset bones */
void PMDModel::resetBone()
{
   unsigned short i;
   btVector3 zeroPos = btVector3(0.0f, 0.0f, 0.0f);
   btQuaternion zeroRot = btQuaternion(0.0f, 0.0f, 0.0f, 1.0f);

   /* set zero position for IK-controlled bones before applying motion */
   for (i = 0; i < m_numBone; i++)
      switch(m_boneList[i].getType()) {
      case UNDER_IK:
      case IK_TARGET:
         m_boneList[i].setCurrentPosition(&zeroPos);
         m_boneList[i].setCurrentRotation(&zeroRot);
         break;
      }
}

/* PMDModel::updateBone: update bones */
void PMDModel::updateBone()
{
   unsigned short i;

   /* update bone matrix from current position and rotation */
   for (i = 0; i < m_numBone; i++)
      m_orderedBoneList[i]->update();

   /* solve IK chains */
   if (m_enableSimulation) {
      /* IK with simulated bones can be skipped */
      for (i = 0; i < m_numIK; i++)
         if (!m_IKSimulated[i]) m_IKList[i].solve();
   } else {
      /* all IK should be solved when simulation is off */
      for (i = 0; i < m_numIK; i++)
         m_IKList[i].solve();
   }

   /* apply under-rotate effects */
   for (i = 0; i < m_numRotateBone; i++)
      m_boneList[m_rotateBoneIDList[i]].updateRotate();
}

/* PMDModel::updateBoneFromSimulation: update bone transform from rigid body */
void PMDModel::updateBoneFromSimulation()
{
   unsigned long i;

   for (i = 0; i < m_numRigidBody; i++)
      m_rigidBodyList[i].applyTransformToBone();
}

/* PMDModel::updateFace: update face morph from current face weights */
void PMDModel::updateFace()
{
   unsigned short i;

   if (m_faceList) {
      m_baseFace->apply(m_vertexList);
      for (i = 0; i < m_numFace; i++)
         if (m_faceList[i].getWeight() > PMDMODEL_MINFACEWEIGHT)
            m_faceList[i].add(m_vertexList, m_faceList[i].getWeight());
   }
}

/* PMDModel::updateSkin: update skin data from bone orientation */
void PMDModel::updateSkin()
{
   unsigned short i;
   unsigned long j;
   btVector3 v, v2, n, n2;

   /* calculate transform matrix for skinning (global -> local) */
   for (i = 0; i < m_numBone; i++)
      m_boneList[i].calcSkinningTrans(&(m_boneSkinningTrans[i]));

   /* do skinning */
   for (j = 0; j < m_numVertex; j++) {
      if (m_boneWeight1[j] >= 1.0f - PMDMODEL_MINBONEWEIGHT) {
         /* bone 1 */
         m_skinnedVertexList[j] = m_boneSkinningTrans[m_bone1List[j]] * m_vertexList[j];
         m_skinnedNormalList[j] = m_boneSkinningTrans[m_bone1List[j]].getBasis() * m_normalList[j];
      } else if (m_boneWeight1[j] <= PMDMODEL_MINBONEWEIGHT) {
         /* bone 2 */
         m_skinnedVertexList[j] = m_boneSkinningTrans[m_bone2List[j]] * m_vertexList[j];
         m_skinnedNormalList[j] = m_boneSkinningTrans[m_bone2List[j]].getBasis() * m_normalList[j];
      } else {
         /* lerp */
         v = m_boneSkinningTrans[m_bone1List[j]] * m_vertexList[j];
         n = m_boneSkinningTrans[m_bone1List[j]].getBasis() * m_normalList[j];
         v2 = m_boneSkinningTrans[m_bone2List[j]] * m_vertexList[j];
         n2 = m_boneSkinningTrans[m_bone2List[j]].getBasis() * m_normalList[j];
         m_skinnedVertexList[j] = v2.lerp(v, m_boneWeight1[j]);
         m_skinnedNormalList[j] = n2.lerp(n, m_boneWeight1[j]);
      }
   }
}

/* PMDModel::updateToon: update toon coordinates and edge vertices */
void PMDModel::updateToon(btVector3 *light)
{
   unsigned long i;

   if (m_toon) {
      /* calculate toon texture coordinates for toon shading */
      for (i = 0; i < m_numVertex; i++) {
         m_toonTexCoordList[i].u = 0.0f;
         m_toonTexCoordList[i].v = (1.0f - light->dot(m_skinnedNormalList[i])) * 0.5f;
      }
      /* calculate vertices for edge drawing */
      for (i = 0; i < m_numVertex; i++) {
         /* not push vertices with no edge flag */
         if (m_noEdgeFlag[i] == 1)
            m_edgeVertexList[i] = m_skinnedVertexList[i];
         else
            m_edgeVertexList[i] = m_skinnedVertexList[i] + m_skinnedNormalList[i] * m_edgeOffset;
      }
   }
}

/* PMDModel::updateShadowColorTexCoord: update / create pseudo toon coordinates for shadow rendering pass on shadow mapping */
void PMDModel::updateShadowColorTexCoord(float coef)
{
   unsigned long i;
   bool update = false;

   if (!m_toon) return;

   if (m_toonTexCoordListForShadowMap == NULL) {
      m_toonTexCoordListForShadowMap = (TexCoord *) malloc(sizeof(TexCoord) * m_numVertex);
      update = true;
   } else if (m_selfShadowDensityCoef != coef) {
      update = true;
   }

   if (update) {
      for (i = 0 ; i < m_numVertex ; i++) {
         m_toonTexCoordListForShadowMap[i].u = 0.0f;
         m_toonTexCoordListForShadowMap[i].v = coef;
      }
      m_selfShadowDensityCoef = coef;
   }
}

/* PMDModel::calculateBoundingSphereRange: calculate the bounding sphere for depth texture rendering on shadow mapping */
float PMDModel::calculateBoundingSphereRange(btVector3 *cpos)
{
   unsigned long i;
   btVector3 centerPos = btVector3(0, 0, 0), v;
   float maxR = 0.0f, r2;

   if (m_centerBone) {
      centerPos = m_centerBone->getTransform()->getOrigin();
      for (i = 0; i < m_numVertex; i += m_boundingSphereStep) {
         r2 = centerPos.distance2(m_skinnedVertexList[i]);
         if (maxR < r2) maxR = r2;
      }
      maxR = sqrtf(maxR) * 1.1f;
   } else {
      maxR = 0.0f;
   }

   if (cpos) *cpos = centerPos;

   return maxR;
}

/* PMDModel::smearAllBonesToDefault: smear all bone pos/rot into default value (rate 1.0 = keep, rate 0.0 = reset) */
void PMDModel::smearAllBonesToDefault(float rate)
{
   unsigned short i;
   const btVector3 v(0.0f, 0.0f, 0.0f);
   const btQuaternion q(0.0f, 0.0f, 0.0f, 1.0f);
   btVector3 tmpv;
   btQuaternion tmpq;

   for (i = 0; i < m_numBone; i++) {
      m_boneList[i].getCurrentPosition(&tmpv);
      tmpv = v.lerp(tmpv, rate);
      m_boneList[i].setCurrentPosition(&tmpv);
      m_boneList[i].getCurrentRotation(&tmpq);
      tmpq = q.slerp(tmpq, rate);
      m_boneList[i].setCurrentRotation(&tmpq);
   }
   for (i = 0; i < m_numFace; i++) {
      m_faceList[i].setWeight(m_faceList[i].getWeight() * rate);
   }
}
