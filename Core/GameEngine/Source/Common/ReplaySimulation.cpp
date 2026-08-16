/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ReplaySimulation.h"

#include "Common/GameEngine.h"
#include "Common/LocalFileSystem.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Recorder.h"
#include "Common/ScoreKeeper.h"
#include "Common/ThingTemplate.h"
#include "Common/WorkerProcess.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameClient/GameClient.h"

// TheSuperHackers @feature Nemellud 07/07/2026 Dump every live object's id/template/position/
// health/owner to a CSV file, for offline analysis of where units actually were on the map
// relative to the map's own geometry (see -dumpObjectState). Called every DUMP_FRAME_INTERVAL
// logic frames rather than every frame to keep the output file a manageable size.
namespace
{
const int DUMP_FRAME_INTERVAL = 30;

void dumpObjectState(FILE *f, const AsciiString &replayName)
{
	if (!f)
		return;
	UnsignedInt frame = TheGameLogic->getFrame();
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const Coord3D *pos = obj->getPosition();
		Real health = -1.0f;
		if (obj->getBodyModule())
			health = obj->getBodyModule()->getHealth();
		Int playerIndex = -1;
		if (obj->getControllingPlayer())
			playerIndex = obj->getControllingPlayer()->getPlayerIndex();
		fprintf(f, "%s,%u,%u,%s,%.1f,%.1f,%.1f,%.1f,%d\n",
			replayName.str(), frame, (unsigned int)obj->getID(),
			obj->getTemplate() ? obj->getTemplate()->getName().str() : "",
			pos->x, pos->y, pos->z, health, playerIndex);
	}
}

// TheSuperHackers @feature Nemellud 14/08/2026 Dump each player's economy every
// DUMP_FRAME_INTERVAL frames, alongside the object dump. supply_income counts only money
// delivered to supply centers, so the harvesting rate can be measured without crates,
// Black Market or Supply Drop Zone income mixed in (total_earned has all of it).
void dumpPlayerState(FILE *f, const AsciiString &replayName)
{
	if (!f || !ThePlayerList)
		return;
	UnsignedInt frame = TheGameLogic->getFrame();
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (!player)
			continue;
		fprintf(f, "%s,%u,%d,%s,%d,%d,%u\n",
			replayName.str(), frame, player->getPlayerIndex(),
			player->getSide().str(),
			player->getMoney() ? (Int)player->getMoney()->countMoney() : 0,
			player->getScoreKeeper()->getTotalMoneyEarned(),
			player->getSupplyIncomeTotal());
	}
}
} // namespace


Bool ReplaySimulation::s_isRunning = false;
UnsignedInt ReplaySimulation::s_replayIndex = 0;
UnsignedInt ReplaySimulation::s_replayCount = 0;

namespace
{
int countProcessesRunning(const std::vector<WorkerProcess>& processes)
{
	int numProcessesRunning = 0;
	size_t i = 0;
	for (; i < processes.size(); ++i)
	{
		if (processes[i].isRunning())
			++numProcessesRunning;
	}
	return numProcessesRunning;
}
} // namespace

int ReplaySimulation::simulateReplaysInThisProcess(const std::vector<AsciiString> &filenames)
{
	int numErrors = 0;

	if (!TheGlobalData->m_headless)
	{
		s_isRunning = true;
		s_replayIndex = 0;
		s_replayCount = static_cast<UnsignedInt>(filenames.size());

		// If we are not in headless mode, we need to run the replay in the engine.
		for (; s_replayIndex < s_replayCount; ++s_replayIndex)
		{
			TheRecorder->playbackFile(filenames[s_replayIndex]);
			TheGameEngine->execute();
			if (TheRecorder->sawCRCMismatch())
				numErrors++;
			if (!s_isRunning)
				break;
			TheGameEngine->setQuitting(FALSE);
		}
		s_isRunning = false;
		s_replayIndex = 0;
		s_replayCount = 0;
		return numErrors != 0 ? 1 : 0;
	}
	// Note that we use printf here because this is run from cmd.
	DWORD totalStartTimeMillis = GetTickCount();

	FILE *dumpFile = nullptr;
	FILE *playerDumpFile = nullptr;
	if (!TheGlobalData->m_dumpObjectStatePath.isEmpty())
	{
		dumpFile = fopen(TheGlobalData->m_dumpObjectStatePath.str(), "w");
		if (dumpFile)
			fprintf(dumpFile, "replay,frame,object_id,template,x,y,z,health,player_index\n");
		else
			printf("Kon dump-bestand niet openen: %s\n", TheGlobalData->m_dumpObjectStatePath.str());

		AsciiString playerDumpPath = TheGlobalData->m_dumpObjectStatePath;
		playerDumpPath.concat(".players.csv");
		playerDumpFile = fopen(playerDumpPath.str(), "w");
		if (playerDumpFile)
			fprintf(playerDumpFile, "replay,frame,player_index,side,money,total_earned,supply_income\n");
		else
			printf("Kon spelerdump-bestand niet openen: %s\n", playerDumpPath.str());
	}

	for (size_t i = 0; i < filenames.size(); i++)
	{
		AsciiString filename = filenames[i];
		printf("Simulating Replay \"%s\"\n", filename.str());
		fflush(stdout);
		DWORD startTimeMillis = GetTickCount();
		if (TheRecorder->simulateReplay(filename))
		{
			UnsignedInt totalTimeSec = TheRecorder->getPlaybackFrameCount() / LOGICFRAMES_PER_SECOND;
			while (TheRecorder->isPlaybackInProgress())
			{
				TheGameClient->updateHeadless();

				const int progressFrameInterval = 10*60*LOGICFRAMES_PER_SECOND;
				if (TheGameLogic->getFrame() != 0 && TheGameLogic->getFrame() % progressFrameInterval == 0)
				{
					// Print progress report
					UnsignedInt gameTimeSec = TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND;
					UnsignedInt realTimeSec = (GetTickCount()-startTimeMillis) / 1000;
					printf("Elapsed Time: %02d:%02d Game Time: %02d:%02d/%02d:%02d\n",
							realTimeSec/60, realTimeSec%60, gameTimeSec/60, gameTimeSec%60, totalTimeSec/60, totalTimeSec%60);
					fflush(stdout);
				}
				TheGameLogic->UPDATE();
				if (TheGameLogic->getFrame() % DUMP_FRAME_INTERVAL == 0)
				{
					if (dumpFile)
						dumpObjectState(dumpFile, filename);
					if (playerDumpFile)
						dumpPlayerState(playerDumpFile, filename);
				}
				if (TheRecorder->sawCRCMismatch())
				{
					numErrors++;
					// TheSuperHackers @feature Nemellud 07/07/2026 When dumping object state
					// for offline analysis (not the original CRC-desync-regression use case),
					// keep simulating past a mismatch instead of aborting - a replay recorded
					// against a different game build than this one is expected to desync
					// eventually, but the position/health data collected before that point
					// (and even somewhat after, since a single mismatch doesn't invalidate the
					// whole rest of the simulation) is still useful.
					if (!dumpFile)
						break;
				}
			}
			UnsignedInt gameTimeSec = TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND;
			UnsignedInt realTimeSec = (GetTickCount()-startTimeMillis) / 1000;
			printf("Elapsed Time: %02d:%02d Game Time: %02d:%02d/%02d:%02d\n",
					realTimeSec/60, realTimeSec%60, gameTimeSec/60, gameTimeSec%60, totalTimeSec/60, totalTimeSec%60);
			fflush(stdout);
		}
		else
		{
			printf("Cannot open replay\n");
			numErrors++;
		}
	}
	if (filenames.size() > 1)
	{
		printf("Simulation of all replays completed. Errors occurred: %d\n", numErrors);

		UnsignedInt realTime = (GetTickCount()-totalStartTimeMillis) / 1000;
		printf("Total Time: %d:%02d:%02d\n", realTime/60/60, realTime/60%60, realTime%60);
		fflush(stdout);
	}

	if (dumpFile)
		fclose(dumpFile);
	if (playerDumpFile)
		fclose(playerDumpFile);

	return numErrors != 0 ? 1 : 0;
}

int ReplaySimulation::simulateReplaysInWorkerProcesses(const std::vector<AsciiString> &filenames, int maxProcesses)
{
	DWORD totalStartTimeMillis = GetTickCount();

	WideChar exePath[1024];
	GetModuleFileNameW(nullptr, exePath, ARRAY_SIZE(exePath));

	std::vector<WorkerProcess> processes;
	int filenamePositionStarted = 0;
	int filenamePositionDone = 0;
	int numErrors = 0;

	while (true)
	{
		int i;
		for (i = 0; i < processes.size(); i++)
			processes[i].update();

		// Get result of finished processes and print output in order
		while (!processes.empty())
		{
			if (!processes[0].isDone())
				break;
			AsciiString stdOutput = processes[0].getStdOutput();
			printf("%d/%d %s", filenamePositionDone+1, (int)filenames.size(), stdOutput.str());
			DWORD exitcode = processes[0].getExitCode();
			if (exitcode != 0)
				printf("Error!\n");
			fflush(stdout);
			numErrors += exitcode == 0 ? 0 : 1;
			processes.erase(processes.begin());
			filenamePositionDone++;
		}

		int numProcessesRunning = countProcessesRunning(processes);

		// Add new processes when we are below the limit and there are replays left
		while (numProcessesRunning < maxProcesses && filenamePositionStarted < filenames.size())
		{
			UnicodeString filenameWide;
			filenameWide.translate(filenames[filenamePositionStarted]);
			UnicodeString command;
			command.format(L"\"%s\"%s%s -replay \"%s\"",
				exePath,
				TheGlobalData->m_windowed ? L" -win" : L"",
				TheGlobalData->m_headless ? L" -headless" : L"",
				filenameWide.str());

			processes.push_back(WorkerProcess());
			processes.back().startProcess(command);

			filenamePositionStarted++;
			numProcessesRunning++;
		}

		if (processes.empty())
			break;

		// Don't waste CPU here, our workers need every bit of CPU time they can get
		Sleep(100);
	}

	DEBUG_ASSERTCRASH(filenamePositionStarted == filenames.size(), ("inconsistent file position 1"));
	DEBUG_ASSERTCRASH(filenamePositionDone == filenames.size(), ("inconsistent file position 2"));

	printf("Simulation of all replays completed. Errors occurred: %d\n", numErrors);

	UnsignedInt realTime = (GetTickCount()-totalStartTimeMillis) / 1000;
	printf("Total Wall Time: %d:%02d:%02d\n", realTime/60/60, realTime/60%60, realTime%60);
	fflush(stdout);

	return numErrors != 0 ? 1 : 0;
}

std::vector<AsciiString> ReplaySimulation::resolveFilenameWildcards(const std::vector<AsciiString> &filenames)
{
	// If some filename contains wildcards, search for actual filenames.
	// Note that we cannot do this in parseReplay because we require TheLocalFileSystem initialized.
	std::vector<AsciiString> filenamesResolved;
	for (std::vector<AsciiString>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename)
	{
		if (filename->find('*') || filename->find('?'))
		{
			AsciiString dir1 = TheRecorder->getReplayDir();
			AsciiString dir2 = *filename;
			AsciiString wildcard = *filename;
			{
				int len = dir2.getLength();
				while (len)
				{
					char c = dir2.getCharAt(len-1);
					if (c == '/' || c == '\\')
					{
						wildcard.set(wildcard.str()+dir2.getLength());
						break;
					}
					dir2.removeLastChar();
					len--;
				}
			}

			FilenameList files;
			TheLocalFileSystem->getFileListInDirectory(dir2.str(), dir1.str(), wildcard, files, FALSE);
			for (FilenameList::iterator it = files.begin(); it != files.end(); ++it)
			{
				AsciiString file;
				file.set(it->str() + dir1.getLength());
				filenamesResolved.push_back(file);
			}
		}
		else
			filenamesResolved.push_back(*filename);
	}
	return filenamesResolved;
}

int ReplaySimulation::simulateReplays(const std::vector<AsciiString> &filenames, int maxProcesses)
{
	std::vector<AsciiString> filenamesResolved = resolveFilenameWildcards(filenames);
	if (maxProcesses == SIMULATE_REPLAYS_SEQUENTIAL)
		return simulateReplaysInThisProcess(filenamesResolved);
	else
		return simulateReplaysInWorkerProcesses(filenamesResolved, maxProcesses);
}
