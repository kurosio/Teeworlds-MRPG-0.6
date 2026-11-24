// Tests of the generic background runtime: Async::Run uses workers independent
// of database endpoint pools, while completions share the main-thread pump.
// AsyncFile provides whole-file I/O that never runs on the game thread.
//
// These tests verify execution behaviour, not just interfaces: where the work
// ran, where the answer arrives, and what ends up on disk when writes are queued
// faster than they can be written.

#include <engine/server/async.h>
#include <engine/server/async_file.h>
#include <engine/storage.h>

#include <base/system.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{

	// Pumps the game-thread completions until the predicate holds or time runs out.
	template<typename TCondition>
	bool PumpUntil(TCondition Condition, int TimeoutMs = 10000)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(TimeoutMs);
		while(std::chrono::steady_clock::now() < Deadline)
		{
			Async::RunMainThreadTasks();
			if(Condition())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		Async::RunMainThreadTasks();
		return Condition();
	}

	// Minimal storage: real files through the io_* layer, so AsyncFile is tested
	// against a real disk and not against a mock of itself.
	class CTestStorage : public IStorageEngine
	{
	public:
		// Counted so a test can prove that queued writes collapse instead of all
		// reaching the disk.
		std::atomic<int> m_Writes { 0 };
		std::atomic<bool> m_FailReads { false };

		int NumPaths() const override { return 1; }
		void ListDirectory(int, const char*, FS_LISTDIR_CALLBACK, void*) override {}
		void ListDirectoryInfo(int, const char*, FS_LISTDIR_CALLBACK_FILEINFO, void*) override {}
		IOHANDLE OpenFile(const char* pFilename, int Flags, int, char* = nullptr, int = 0) override
		{
			if(Flags & IOFLAG_WRITE)
			{
				m_Writes++;
				// a disk is slower than the game thread queues writes
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return io_open(pFilename, Flags);
		}
		bool FileExists(const char* pFilename, int) override { return fs_is_file(pFilename) != 0; }
		bool FolderExists(const char*, int) override { return true; }
		bool ReadFile(const char* pFilename, int, void** ppResult, unsigned* pResultLen) override
		{
			if(m_FailReads.load())
			{
				*ppResult = nullptr;
				*pResultLen = 0;
				return false;
			}

			IOHANDLE File = io_open(pFilename, IOFLAG_READ);
			if(!File)
			{
				*ppResult = nullptr;
				*pResultLen = 0;
				return false;
			}

			io_read_all(File, ppResult, pResultLen);
			io_close(File);
			return true;
		}
		char* ReadFileStr(const char* pFilename, int Type) override
		{
			void* pResult = nullptr;
			unsigned Len = 0;
			if(!ReadFile(pFilename, Type, &pResult, &Len))
				return nullptr;
			return static_cast<char*>(pResult);
		}
		bool CalculateHashes(const char*, int, SHA256_DIGEST*, unsigned*) override { return false; }
		bool FindFile(const char*, const char*, int, char*, int) override { return false; }
		size_t FindFiles(const char*, const char*, int, std::set<std::string>*) override { return 0; }
		bool RemoveFile(const char* pFilename, int) override { return fs_remove(pFilename) == 0; }
		bool RemoveFolder(const char*, int) override { return false; }
		bool RenameFile(const char*, const char*, int) override { return false; }
		bool CreateFolder(const char*, int) override { return true; }
		void GetCompletePath(int, const char* pDir, char* pBuffer, unsigned BufferSize) override { str_copy(pBuffer, pDir, BufferSize); }
		bool RemoveBinaryFile(const char*) override { return false; }
		bool RenameBinaryFile(const char*, const char*) override { return false; }
		const char* GetBinaryPath(const char* pFilename, char* pBuffer, unsigned BufferSize) override
		{
			str_copy(pBuffer, pFilename, BufferSize);
			return pBuffer;
		}
		const char* GetBinaryPathAbsolute(const char* pFilename, char* pBuffer, unsigned BufferSize) override
		{
			str_copy(pBuffer, pFilename, BufferSize);
			return pBuffer;
		}
	};

	std::string ReadAll(const char* pFilename)
	{
		IOHANDLE File = io_open(pFilename, IOFLAG_READ);
		if(!File)
			return std::string {};

		char* pData = io_read_all_str(File);
		io_close(File);
		if(!pData)
			return std::string {};

		std::string Result(pData);
		free(pData);
		return Result;
	}

	void WriteRaw(const char* pFilename, const char* pContent)
	{
		IOHANDLE File = io_open(pFilename, IOFLAG_WRITE);
		if(!File)
			return;
		io_write(File, pContent, (unsigned)str_length(pContent));
		io_close(File);
	}
}

// ---------------------------------------------------------------------------
// Async::Run
// ---------------------------------------------------------------------------
class CAsyncRuntimeTest : public ::testing::Test
{
protected:
	void SetUp() override { Async::StartMainThreadTasks(); }
	void TearDown() override
	{
		Async::Drain();
		Async::StopMainThreadTasks();
	}
};

class CAsyncRunTest : public CAsyncRuntimeTest
{
};

class CAsyncFileTest : public CAsyncRuntimeTest
{
};

TEST_F(CAsyncRunTest, WorkRunsOffTheGameThreadAndTheAnswerArrivesOnIt)
{
	const auto MainThread = std::this_thread::get_id();
	std::atomic<int> WorkDone { 0 };
	std::atomic<int> Answers { 0 };
	std::atomic<int> WorkOnMainThread { 0 };
	std::atomic<int> AnswerOffMainThread { 0 };

	constexpr int Jobs = 64;
	for(int i = 0; i < Jobs; ++i)
	{
		Async::Run([&, MainThread]
		{
			if(std::this_thread::get_id() == MainThread)
				WorkOnMainThread++;
			WorkDone++;
		},
		[&, MainThread]
		{
			if(std::this_thread::get_id() != MainThread)
				AnswerOffMainThread++;
			Answers++;
		});
	}

	EXPECT_TRUE(PumpUntil([&] { return Answers.load() == Jobs; })) << "only " << Answers.load() << " of " << Jobs << " answers arrived";
	EXPECT_EQ(WorkDone.load(), Jobs);
	EXPECT_EQ(WorkOnMainThread.load(), 0) << "background work must not run on the game thread";
	EXPECT_EQ(AnswerOffMainThread.load(), 0) << "answers must run on the game thread";
}

TEST_F(CAsyncRunTest, ThrowingWorkStillAnswers)
{
	std::atomic<int> Answers { 0 };
	Async::Run([] { throw std::runtime_error("expected failure"); }, [&] { Answers++; });
	Async::Run([] {}, [&] { Answers++; });

	EXPECT_TRUE(PumpUntil([&] { return Answers.load() == 2; }));
}

TEST_F(CAsyncRunTest, AnswersCanQueueMoreWork)
{
	std::atomic<int> Stages { 0 };
	Async::Run([&]
	{
		Async::Run([&]
		{
			Async::Run([&] {}, [&] { Stages = 3; });
		}, [&] { if(Stages == 0) Stages = 2; });
	}, [&] { Stages = 1; });

	EXPECT_TRUE(PumpUntil([&] { return Stages.load() == 3; })) << "stages reached: " << Stages.load();
}

TEST_F(CAsyncRunTest, SubmittingWaitsForNothing)
{
	std::atomic<bool> Started { false };
	std::atomic<bool> Release { false };
	std::atomic<int> Answers { 0 };

	// The work stays gated until this test releases it, so the call can only
	// return early if queueing does not wait for the work.
	const auto Begin = std::chrono::steady_clock::now();
	Async::Run([&]
	{
		Started = true;
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while(!Release.load() && std::chrono::steady_clock::now() < Deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	},
		[&] { Answers++; });
	const auto SubmitMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Begin).count();

	EXPECT_LT(SubmitMs, 1000) << "queueing work must return at once, not wait for it";
	EXPECT_TRUE(PumpUntil([&] { return Started.load(); }));
	EXPECT_EQ(Answers.load(), 0) << "the answer must wait for the work";

	Release = true;
	EXPECT_TRUE(PumpUntil([&] { return Answers.load() == 1; }));
}

TEST_F(CAsyncRunTest, GenericJobsUseTheirOwnWorkerQueue)
{
	std::atomic<bool> Started { false };
	std::atomic<bool> Release { false };
	std::atomic<int> Work { 0 };
	std::atomic<int> Answers { 0 };
	std::atomic<int> WorkOnMainThread { 0 };
	const auto MainThread = std::this_thread::get_id();

	Async::Run([&]
	{
		Started = true;
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while(!Release.load() && std::chrono::steady_clock::now() < Deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		Work++;
	}, [&] { Answers++; });
	ASSERT_TRUE(PumpUntil([&] { return Started.load(); }));

	// Other generic jobs progress independently while one worker is occupied.
	Async::Run([&] { if(std::this_thread::get_id() == MainThread) WorkOnMainThread++; Work++; }, [&] { Answers++; });
	Async::Run([&] { if(std::this_thread::get_id() == MainThread) WorkOnMainThread++; Work++; }, [&] { Answers++; });
	EXPECT_EQ(Answers.load(), 0);
	EXPECT_EQ(WorkOnMainThread.load(), 0);

	Release = true;
	EXPECT_TRUE(PumpUntil([&] { return Answers.load() == 3; }));
	EXPECT_EQ(Work.load(), 3);
	EXPECT_EQ(WorkOnMainThread.load(), 0);
}

TEST_F(CAsyncRunTest, ShutdownDropsAnswersThatWereNeverDelivered)
{
	std::atomic<int> Work { 0 };
	std::atomic<int> Answers { 0 };
	Async::Run([&] { Work++; }, [&] { Answers++; });

	// Wait for the work without pumping the game thread, so the answer is queued
	// but not delivered when the shutdown starts.
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while(Work.load() == 0 && std::chrono::steady_clock::now() < Deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	ASSERT_EQ(Work.load(), 1);

	Async::Drain();
	Async::StopMainThreadTasks();
	Async::RunMainThreadTasks();

	EXPECT_EQ(Answers.load(), 0) << "an answer that arrives during the shutdown must not reach a deleted game object";
}

TEST(CAsyncRunWithoutDatabaseTest, UsesIndependentWorkersAndGameThreadCompletions)
{
	Async::StartMainThreadTasks();

	std::atomic<int> Work { 0 };
	std::atomic<int> Answer { 0 };
	std::atomic<int> FailedAnswer { 0 };
	Async::Run([&] { Work++; }, [&] { Answer++; });
	Async::Run([] { throw std::runtime_error("expected background failure"); }, [&] { FailedAnswer++; });

	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while(Work.load() == 0 && std::chrono::steady_clock::now() < Deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_EQ(Work.load(), 1);
	EXPECT_EQ(Answer.load(), 0) << "completion is queued for the main-thread pump";
	EXPECT_TRUE(PumpUntil([&] { return Answer.load() == 1 && FailedAnswer.load() == 1; }));
	EXPECT_NO_THROW(Async::Run([] {}, [] { throw std::runtime_error("expected completion failure"); }));
	EXPECT_TRUE(PumpUntil([] { return Async::PendingMainThreadTasks() == 0; }));
	Async::Drain();
	Async::StopMainThreadTasks();
}

// ---------------------------------------------------------------------------
// AsyncFile
// ---------------------------------------------------------------------------
TEST_F(CAsyncFileTest, WritesKeepTheNewestContentAndReadSeesIt)
{
	CTestStorage Storage;
	const char* pFilename = "test_async_file_write.txt";
	std::remove(pFilename);

	// Queued faster than they can be written: the last one has to win.
	for(int i = 0; i < 50; ++i)
		AsyncFile::Write(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, "content " + std::to_string(i));

	// Nothing is on disk before the first write task ran, and the newest content
	// is the one that ends there.
	EXPECT_TRUE(PumpUntil([&] { return ReadAll(pFilename) == "content 49"; })) << "file holds: " << ReadAll(pFilename);

	std::string Content;
	std::atomic<bool> Loaded { false };
	AsyncFile::Read(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, [&](AsyncFile::SReadResult&& Result)
	{
		EXPECT_TRUE(Result.Ok()) << Result.m_Error;
		Content = std::move(Result.m_Content);
		Loaded = true;
	});

	EXPECT_TRUE(PumpUntil([&] { return Loaded.load(); }));
	EXPECT_EQ(Content, "content 49");

	// The queue collapsed: far fewer writes reached the disk than were requested,
	// and the last content is the one that stayed.
	EXPECT_LT(Storage.m_Writes.load(), 50);
	std::remove(pFilename);
}

TEST_F(CAsyncFileTest, RemoveDoesNotOvertakeQueuedWrites)
{
	CTestStorage Storage;
	const char* pFilename = "test_async_file_remove.txt";
	WriteRaw(pFilename, "old content");

	AsyncFile::Write(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, "new content");
	AsyncFile::Remove(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE);

	// The removal runs after the write, so the file is gone whichever of the two
	// the worker reaches first.
	EXPECT_TRUE(PumpUntil([&] { return fs_is_file(pFilename) == 0; }));
	EXPECT_FALSE(fs_is_file(pFilename));
}

TEST_F(CAsyncFileTest, MissingFilesAreDifferentFromReadErrors)
{
	CTestStorage Storage;
	AsyncFile::SReadResult Result;
	std::atomic<bool> Loaded { false };
	AsyncFile::Read(&Storage, "test_async_file_missing.txt", IStorageEngine::TYPE_ABSOLUTE,
		[&](AsyncFile::SReadResult&& Read)
		{
			Result = std::move(Read);
			Loaded = true;
		});

	EXPECT_TRUE(PumpUntil([&] { return Loaded.load(); }));
	EXPECT_FALSE(Result.Ok());
	EXPECT_TRUE(Result.Missing());
	EXPECT_EQ(Result.m_Error, "file not found");

	const char* pFilename = "test_async_file_read_error.txt";
	WriteRaw(pFilename, "present but unreadable");
	Storage.m_FailReads = true;
	Loaded = false;
	AsyncFile::Read(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE,
		[&](AsyncFile::SReadResult&& Read)
		{
			Result = std::move(Read);
			Loaded = true;
		});
	EXPECT_TRUE(PumpUntil([&] { return Loaded.load(); }));
	EXPECT_FALSE(Result.Ok());
	EXPECT_FALSE(Result.Missing());
	EXPECT_EQ(Result.m_Error, "read failed");
	std::remove(pFilename);
}

TEST_F(CAsyncFileTest, TheCallsReturnBeforeTheAnswerArrives)
{
	CTestStorage Storage;
	const char* pFilename = "test_async_file_pending.txt";
	std::remove(pFilename);

	int Answers = 0;
	AsyncFile::Write(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, "pending");

	// Write is asynchronous, but Read is an independent operation and is not
	// specified to wait for earlier writes. Wait for the file before requesting
	// its contents so this test does not rely on worker scheduling order.
	EXPECT_TRUE(PumpUntil([&] { return fs_is_file(pFilename) != 0; }));
	AsyncFile::Read(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, [&](AsyncFile::SReadResult&& Result)
	{
		EXPECT_TRUE(Result.Ok()) << Result.m_Error;
		Answers++;
	});

	// The read answer belongs to the runtime, not to the caller.
	EXPECT_EQ(Answers, 0) << "a file answer must never run inside the call";
	EXPECT_TRUE(PumpUntil([&] { return Answers == 1; }));

	std::atomic<bool> Loaded { false };
	std::string Content;
	AsyncFile::Read(&Storage, pFilename, IStorageEngine::TYPE_ABSOLUTE, [&](AsyncFile::SReadResult&& Result)
	{
		EXPECT_TRUE(Result.Ok()) << Result.m_Error;
		Content = std::move(Result.m_Content);
		Loaded = true;
	});
	EXPECT_TRUE(PumpUntil([&] { return Loaded.load(); }));
	EXPECT_EQ(Content, "pending");
	std::remove(pFilename);
}
