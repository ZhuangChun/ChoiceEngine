#include "Graphics.h"

HINSTANCE GHInstance;
CGraphics::CGraphics(void)
{
}


CGraphics::~CGraphics(void)
{
}

bool CGraphics::InitGDI( HINSTANCE hinstance )
{
	GHInstance = hinstance;

	// CreateCompatibleDC���������Ļ���(�ڴ���ʾ�豸) CreateCompatibleBitmap����λͼ
	mBufferedHDC = ::CreateCompatibleDC(NULL);

	////////////////////
	BITMAPINFO info					= {0};    
	info.bmiHeader.biSize			= sizeof(info.bmiHeader);    
	info.bmiHeader.biWidth			= SCREEN_WIDTH;
	info.bmiHeader.biHeight			= -SCREEN_HEIGHT;    
	info.bmiHeader.biPlanes			= 1;    
	info.bmiHeader.biBitCount		= 32;    
	info.bmiHeader.biCompression	= BI_RGB;    
	info.bmiHeader.biSizeImage		= SCREEN_WIDTH * SCREEN_HEIGHT * 32 / 8;  

	// ����һ���ڴ���������ȡ������ָ��
	void* pBits = NULL;
	mBufferedHandle = ::CreateDIBSection(mBufferedHDC, &info, DIB_RGB_COLORS, &pBits, NULL, 0);
	::SelectObject(mBufferedHDC, mBufferedHandle);

	//here: "dib.dsBm.bmBits" will points to the pixels of hdib.
	::GetObject(mBufferedHandle , sizeof(DIBSECTION), &mDIBSection);
	mDatas = (BYTE*)mDIBSection.dsBm.bmBits;
	mPitch = mDIBSection.dsBm.bmWidthBytes;

	// ����ˢ�������С
	::SetRect(&mBufferSize, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	//GPen = (HPEN)::GetStockObject(WHITE_PEN); 
	mPen = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); 
	::SelectObject(mBufferedHDC, mPen);

	mBgBrush= ::CreateSolidBrush(RGB(0, 0, 255));
	::SelectObject (mBufferedHDC, mBgBrush) ;

	// ��������
	HFONT hfnt = (HFONT)::GetStockObject(OEM_FIXED_FONT); 
	::SelectObject(mBufferedHDC, hfnt);
	// �������ֱ���Ϊ͸��ɫ
	::SetBkMode(mBufferedHDC, TRANSPARENT); 

	mZBuffer = new float[SCREEN_WIDTH * SCREEN_HEIGHT];
	memset(mZBuffer, 0, sizeof(float) * SCREEN_WIDTH * SCREEN_HEIGHT);

	return true;
}

// �رջ�ͼϵͳ
void CGraphics::ShutdownGraphicss()
{
	::DeleteObject(mPen);
	::DeleteObject(mBgBrush);
	::DeleteObject(mBufferedHandle);
	::DeleteDC(mBufferedHDC);

	//SafeDeleteArray(mFaces);
	//SafeDeleteArray(mVertexs);
}

void CGraphics::BuildMatrix()
{
	assert(m_pCamera != NULL);
	// ���������ϵ�任����
	Matrix4::ViewMatrix(mMatrix[TS_VIEW], m_pCamera->m_WorldPos, m_pCamera->m_LookAt, m_pCamera->m_Up);
	// ͸������ϵ�任����
	Matrix4::ProjectMatrix(mMatrix[TS_PROJECT],	Degree_TO_Radian(m_pCamera->m_Fov), 
		(float)GetScreenWidth() / (float)GetScreenHeight(), 
		m_pCamera->m_Near, m_pCamera->m_Far);
	// ��Ļ����ϵ�任����
	Matrix4::ScreenMatrix(mMatrix[TS_SCRREN], GetScreenWidth(), GetScreenHeight());
}

void CGraphics::TranslateToViewSpace()
{
	Matrix4 mat = mMatrix[TS_LOCAL] * mMatrix[TS_WORLD] * mMatrix[TS_VIEW];
	for (int i = 0; i < mVertexNum; i++)
	{
		CVertex& ver = mVertexs[i];
		// ����任���۲�����ϵ
		ver.mVertex = Vec4MulMat4W(ver.mVertex, mat);
		// ���㷨��
		ver.mNormal = Vec4MulMat4W(ver.mNormal, mat);

		ver.mVertexView = ver.mVertex;
	}

	Matrix4 invViewMat;
	InverseMatrix4(invViewMat, mMatrix[TS_VIEW]);
	Matrix4 invWorldMat;
	InverseMatrix4(invWorldMat, mMatrix[TS_WORLD]);
	Matrix4 invLocalMat;
	InverseMatrix4(invLocalMat, mMatrix[TS_LOCAL]);
	m_matWorld2Local = invViewMat * invWorldMat * invLocalMat;
}

void CGraphics::TranslateToProjectiveSpace()
{
	for (int i = 0; i < mVertexNum; i++)
	{
		CVertex& ver = mVertexs[i];
		ver.mVertex = Vec4MulMat4W(ver.mVertex, mMatrix[TS_PROJECT]);
		// ִ��͸�ӳ���, ���������任���ѿ���������ϵ
		ver.mVertex /= ver.mVertex.w;
	}
}

void CGraphics::TranslateToScreenSpace( const CFace& face, Vector4& v0, Vector4& v1, Vector4& v2 )
{
	v0 = Vec4MulMat4(mVertexs[face.mVertIndex[0]].mVertex, mMatrix[TS_SCRREN]);
	v1 = Vec4MulMat4(mVertexs[face.mVertIndex[1]].mVertex, mMatrix[TS_SCRREN]);
	v2 = Vec4MulMat4(mVertexs[face.mVertIndex[2]].mVertex, mMatrix[TS_SCRREN]);
}

void CGraphics::SetVertex( CVertex* vertexs, int vertexNum )
{
	if (mVertexNum != vertexNum)
	{
		delete[] mVertexs;
		mVertexs = new CVertex[vertexNum];
		mVertexNum = vertexNum;
	}	
	memcpy(mVertexs, vertexs, sizeof(CVertex) * vertexNum);
}

void CGraphics::SetCamera( CCamera* camera )
{
	assert(camera);
	m_pCamera = camera;
}

void CGraphics::SetTransform( ETS_Transform space, const Matrix4& mat )
{
	assert(space < TS_NUM && space >= TS_LOCAL);
	mMatrix[space] = mat;
}

void CGraphics::SetTexture( /*int index, */CTexture* texture )
{
	assert(texture);
	mTextures = texture;
}

void CGraphics::SetLight( /*int index, */CLight* light )
{
	assert(light);
	mLights = light;
}

const static float DARK_COLOR_FACTOR = 0.05f;
void CGraphics::ProcessLight()
{
	CLight* pLight = mLights;
	if(pLight != NULL && pLight->m_bEnable)
	{
		// Ŀǰ��һյ��
		mLightPositionView = Vec4MulMat4(pLight->m_dWorldPos, mMatrix[TS_VIEW]);

		if(mShadeType == SHADE_FLAT) // ƽ����ɫ
		{
			for(int i = 0; i < mFaceNum; ++i)
			{
				CFace& pFace = mFaces[i];
				if(!pFace.isCulled)
				{
					Vector4 edge0 = mVertexs[pFace.mVertIndex[0]].mVertex - mVertexs[pFace.mVertIndex[1]].mVertex;
					Vector4 edge1 = mVertexs[pFace.mVertIndex[0]].mVertex - mVertexs[pFace.mVertIndex[2]].mVertex;

					Vector4 faceNormal = edge0.CrossVector(edge1);
					Vector4 lightVect = (mLightPositionView - mVertexs[pFace.mVertIndex[0]].mVertex).Nomalize();

					// ��ɫϵ��
					float dot = faceNormal.DotVector(lightVect);
					if(dot < 0)
						dot = DARK_COLOR_FACTOR;

					pFace.mColor[0] *= dot;
					pFace.mColor[1] *= dot;
					pFace.mColor[2] *= dot;
				}
			}
		}
		else if(SHADE_GAUROUD == mShadeType)
		{
			// �������������ֱ������߼н�
			for(int i = 0; i < mFaceNum; ++i)
			{
				CFace& face = mFaces[i];
				if(!face.isCulled)
				{
					Vector4 v0Normal = mVertexs[mFaces[i].mVertIndex[0]].mVertex.Nomalize();
					Vector4 v1Normal = mVertexs[mFaces[i].mVertIndex[1]].mVertex.Nomalize();
					Vector4 v2Normal = mVertexs[mFaces[i].mVertIndex[2]].mVertex.Nomalize();

					Vector4 lightDir0 = (mLightPositionView - mVertexs[mFaces[i].mVertIndex[0]].mVertex).Nomalize();
					Vector4 lightDir1 = (mLightPositionView - mVertexs[mFaces[i].mVertIndex[1]].mVertex).Nomalize();
					Vector4 lightDir2 = (mLightPositionView - mVertexs[mFaces[i].mVertIndex[2]].mVertex).Nomalize();

					float dot = 1.0f;
					dot = v0Normal.DotVector(lightDir0);
					if(dot < 0)
						dot = DARK_COLOR_FACTOR;
					mFaces->mColor[0] *= dot;

					dot = v1Normal.DotVector(lightDir1);
					if(dot < 0)
						dot = DARK_COLOR_FACTOR;
					mFaces->mColor[1] *= dot;

					dot = v2Normal.DotVector(lightDir2);
					if(dot < 0)
						dot = DARK_COLOR_FACTOR;
					mFaces->mColor[2] *= dot;
				}
			}
		}
		else if(SHADE_PHONG_Vertex == mShadeType)
		{
			for(int i = 0; i < mFaceNum; ++i)
			{
				CFace& face = mFaces[i];
				if(!face.isCulled)
				{
					Vector4 pos0 = mVertexs[face.mVertIndex[0]].mVertex;
					Vector4 normal0 = mVertexs[face.mVertIndex[0]].mNormal;
					face.mColor[0] = PhongCal(pLight, mLightPositionView, pos0, normal0);

					Vector4 pos1 = mVertexs[face.mVertIndex[1]].mVertex;
					Vector4 normal1 = mVertexs[face.mVertIndex[1]].mNormal;
					face.mColor[1] = PhongCal(pLight, mLightPositionView, pos1, normal1);

					Vector4 pos2 = mVertexs[face.mVertIndex[2]].mVertex;
					Vector4 normal2 = mVertexs[face.mVertIndex[2]].mNormal;
					face.mColor[2] = PhongCal(pLight, mLightPositionView, pos2, normal2);
				}
			}
		}
	}
}

void CGraphics::SetFace( CFace* faces, int faceNum )
{
	if (mFaceNum != faceNum)
	{
		delete[] mFaces;
		mFaces = new CFace[faceNum];
		mFaceNum = faceNum;
	}
	memcpy(mFaces, faces, sizeof(CFace) * faceNum);
	mFaceNum += faceNum;
}

CE::Core::CColor CGraphics::PhongCal( CLight* pLight, const Vector4& lightPos, const Vector4& vertexPosView, const Vector4& normal )
{
	if(!pLight)
		return CColor(255, 255, 255);

	// �����׼��
	Vector4 n = normal.Nomalize();
	// ���㵽��Դ������
	Vector4 l = (lightPos - vertexPosView).Nomalize();
	// ���㵽�۲������(���λ��ԭ��)
	Vector4 v = (Vector4(0,0,0,0) - vertexPosView).Nomalize();
	// ������������ο��ʼ�
	Vector4 r = (n * ( 2 * Max(l.DotVector(n), 0)) - l).Nomalize();

	// phoneģ�ͼ��㹫ʽ
	CColor diffuse = pLight->m_dDiffuse * (pLight->m_fKDiffuse * Max(l.DotVector(n), DARK_COLOR_FACTOR));
	CColor respec = pLight->m_dSpecular * (pLight->m_fKSpecular * pow(Max(v.DotVector(r), 0), pLight->m_fShininess));

	return diffuse + respec;
}

void CGraphics::ProcessRasterize()
{
	// ��դ��

}

void CGraphics::DrawPrimitives()
{
	BuildMatrix();

	TranslateToViewSpace();

	ProcessLight();

	TranslateToProjectiveSpace();

	ProcessRasterize();
}

void	CGraphics::ClearBuffer(const CColor& c)
{
	::FillRect(mBufferedHDC, &mBufferSize, mBgBrush);
	// ������Ȼ���
	// ע : ����memsetֻ�ܶ�Int���͵�������г�ʼ��, ��������ֱ��ʹ����Int����
	// ��û��ʹ��float����, Ӧ��ʹ��float
	::memset(mZBuffer, 0, sizeof(float) * SCREEN_WIDTH * SCREEN_HEIGHT);
}

void CGraphics::FlipBuffer(HDC hdc)
{
	// ���Ѿ����ƺõĻ������ݽ���Graphics����Ļ�ϻ���, ������ǰ����������Ϊ��һ��������
	::BitBlt(hdc,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,mBufferedHDC,0,0,SRCCOPY);
}