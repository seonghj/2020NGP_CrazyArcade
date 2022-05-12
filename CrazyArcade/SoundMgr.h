#pragma once

//��ü�� n���� �����ϵ��� ������ �ϴ� ������ �����Դϴ�.
class CSoundMgr
{
private:
	static CSoundMgr* m_pInstance;

public:
	static CSoundMgr* GetInstance(void)
	{
		if (m_pInstance == NULL)
		{
			m_pInstance = new CSoundMgr;
		}
		return m_pInstance;
	}

	static void DestroyInstance(void)
	{
		if (m_pInstance)
		{
			delete m_pInstance;
			m_pInstance = NULL;
		}
	}

	//Fmod �������̽�
private:
	FMOD_SYSTEM*	System;			//fmod �ý��� ������
	FMOD_CHANNEL*	Effect;
	FMOD_CHANNEL*	Effect2;
	FMOD_CHANNEL*	BGM;
	FMOD_CHANNEL*	Skill;
	FMOD_RESULT		Result;		//�����ߵǴ��� Ȯ���ϴ� ����.

	unsigned int m_iVersion;

	map<TCHAR*, FMOD_SOUND*>		m_mapSound;		//���� ����(map)

public:
	void Initialize(void);
	void LoadSoundFile(void);
	void PlaySound(TCHAR* pSoundKey);
	void PlayBGM(TCHAR* pSoundKey);
	void PlayEffectSound(TCHAR* pSoundKey);
	void PlayEffectSound2(TCHAR* pSoundKey);
	void StopSoundAll(void);
	void StopBGMSound(void);
	void StopEffectSound(void);
	void StopEffectSound2(void);

public:
	map<TCHAR*, FMOD_SOUND*>* GetSoundMap(void)
	{
		return &m_mapSound;
	}

private:
	void ErrorCheck(FMOD_RESULT _result);

private:
	CSoundMgr(void);
public:
	~CSoundMgr(void);
};
