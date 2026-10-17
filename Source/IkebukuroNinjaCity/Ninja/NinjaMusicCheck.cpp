#include "NinjaMusicCheck.h"
#include "NinjaMatchDirector.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaMusicCheck::ANinjaMusicCheck()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
}
void ANinjaMusicCheck::BeginPlay()
{
	Super::BeginPlay(); Started=PhaseWall=FPlatformTime::Seconds();
	bFullTrack=FParse::Param(FCommandLine::Get(),TEXT("MusicFullTrackCheck"));
}
void ANinjaMusicCheck::HoldEnemies()
{
	for (ARedNinja* Enemy:Match->Enemies) { Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->StopMovementImmediately(); }
}
void ANinjaMusicCheck::Next(const FString& Case)
{
	Cases.Add(Case); UE_LOG(LogTemp,Display,TEXT("MUSIC_CASE_PASSED %s"),*Case); ++Phase; PhaseWall=FPlatformTime::Seconds();
}
void ANinjaMusicCheck::DefeatAtSongEnd()
{
	if (Phase!=10 || !Match->CanPlay()) return;
	for (ARedNinja* Enemy:Match->Enemies) if (!Enemy->bDefeated) bDeadlineTieDefeated|=Enemy->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja);
}
void ANinjaMusicCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds();
	if (bFinished) { if (Wall-FinishedAt>2) FPlatformMisc::RequestExit(false); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Match || !Ninja) return;
		Match->GetRoundMusicSound()->SetEnableEnvelopeFollowing(true);
		Match->GetRoundMusicSound()->SetVolumeMultiplier(.06f); // Keep automated playback quiet, with a real mixer.
	}
	PeakEnvelope=FMath::Max(PeakEnvelope,Match->GetRoundMusicSound()->GetEnvelopeValue());
	if (Wall-Started>Match->RoundDurationSeconds+100 || (Phase!=4 && Wall-PhaseWall>75)) { Finish(FString::Printf(TEXT("Phase %d timeout: %s"),Phase,*Match->MusicError)); return; }
	auto* Player=Match->GetRoundMusicPlayer();
	if (Phase==0 && FParse::Param(FCommandLine::Get(),TEXT("MusicMissingFileCheck")) && !Match->MusicError.IsEmpty())
	{
		if (Match->State!=ENinjaMatchState::Loading || (Player && Player->IsPlaying())) { Finish(TEXT("Missing music allowed gameplay")); return; }
		Cases.Add(TEXT("Missing local music keeps gameplay stopped and exposes an Enter-to-retry error")); Finish(FString()); return;
	}
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		if (!Match->UsesRoundMusic() || !Player || Player->IsPlaying() || FMath::Abs(Match->RoundDurationSeconds-243.72f)>.15 || Player->GetTime().GetTotalSeconds()>.05) { Finish(TEXT("Music was not prepared at the start without playback")); return; }
		HoldEnemies(); Match->Confirm(); Next(TEXT("Real M4A is ready, duration matches metadata, playback remains stopped before countdown"));
	}
	else if (Phase==1)
	{
		if (Match->State==ENinjaMatchState::Countdown && Wall-PhaseWall>.5 && !bCountdownChecked)
		{
			if (Player->IsPlaying() || Player->GetTime().GetTotalSeconds()>.05) { Finish(TEXT("Music played during countdown")); return; }
			bCountdownChecked=true; Cases.Add(TEXT("Music does not start during the three-second countdown"));
		}
		if (Match->CanPlay() && Player && Player->GetTime().GetTotalSeconds()>.4)
		{
			if (!bCountdownChecked || !Player->IsPlaying() || !Match->GetRoundMusicSound()->IsPlaying() || FMath::Abs(Match->RemainingSeconds-(Match->RoundDurationSeconds-Player->GetTime().GetTotalSeconds()))>.2) { Finish(TEXT("Music, synth and timer did not start together")); return; }
			FullTrackStarted=Wall-Player->GetTime().GetTotalSeconds();
			Match->TogglePause(); PausedPosition=Player->GetTime().GetTotalSeconds(); PausedRemaining=Match->RemainingSeconds;
			Next(TEXT("Gameplay starts the media and audio mixer, HUD remaining time follows media position"));
		}
	}
	else if (Phase==2 && Wall-PhaseWall>.8)
	{
		if (!UGameplayStatics::IsGamePaused(this) || FMath::Abs(Player->GetTime().GetTotalSeconds()-PausedPosition)>.06 || Match->RemainingSeconds!=PausedRemaining) { Finish(TEXT("Paused music or timer advanced")); return; }
		Match->TogglePause(); Next(TEXT("Pause freezes both song position and game timer, then resumes them"));
	}
	else if (Phase==3 && Player->GetTime().GetTotalSeconds()>PausedPosition+.35 && (PeakEnvelope>.00001f || Wall-PhaseWall>5))
	{
		if (PeakEnvelope<=.00001f) { Finish(TEXT("No non-zero decoded PCM reached the audio mixer")); return; }
		if (!bFullTrack && !Player->Seek(Player->GetDuration()-FTimespan::FromMilliseconds(600))) { Finish(TEXT("End seek rejected")); return; }
		Next(bFullTrack?TEXT("Non-zero decoded audio confirmed; play the entire song to its natural end"):TEXT("Non-zero decoded audio confirmed; seek near the end to check the actual end callback"));
	}
	else if (Phase==4 && Match->State==ENinjaMatchState::Lost)
	{
		if (Match->ResultReason!=TEXT("The song has ended") || Player || Ninja->Health!=3 || Match->RemainingSeconds!=0) { Finish(TEXT("Song end did not stop audio and produce the intended loss")); return; }
		if (!bEndCaptured)
		{
			CompletedTrackWallSeconds=Wall-FullTrackStarted;
			if (bFullTrack && CompletedTrackWallSeconds<Match->RoundDurationSeconds-.25) { Finish(TEXT("Full-track loss occurred before a full song elapsed")); return; }
			FString Shot; if (FParse::Value(FCommandLine::Get(),TEXT("MusicScreenshot="),Shot)) FScreenshotRequest::RequestScreenshot(Shot,false,false);
			bEndCaptured=true; PhaseWall=Wall; return;
		}
		if (Wall-PhaseWall<.25) return;
		Match->Confirm(); Next(TEXT("Actual media end produces a time-limit loss and closes the player"));
	}
	else if (Phase==5 && Match->State==ENinjaMatchState::Ready)
	{
		if (!Player || Player->IsPlaying() || Player->GetTime().GetTotalSeconds()>.05 || FMath::Abs(Match->RemainingSeconds-Match->RoundDurationSeconds)>.05 || Match->RoundNumber!=2) { Finish(TEXT("Retry did not reset media to the beginning")); return; }
		HoldEnemies(); Match->Confirm(); Next(TEXT("Retry prepares a fresh media player at zero with the full song duration"));
	}
	else if (Phase==6 && Match->CanPlay() && Player->GetTime().GetTotalSeconds()>.3)
	{
		if (Player->GetTime().GetTotalSeconds()>1.5) { Finish(TEXT("Retried song did not start at the beginning")); return; }
		Ninja->Health=1; Ninja->ReceiveContactDamage(); Next(TEXT("Retry starts the song from the beginning; apply lethal contact damage"));
	}
	else if (Phase==7 && Match->State==ENinjaMatchState::Lost)
	{
		if (Player || Match->ResultReason!=TEXT("No health remaining")) { Finish(TEXT("Early loss did not stop music")); return; }
		Match->Confirm(); Next(TEXT("Health loss stops the song immediately"));
	}
	else if (Phase==8 && Match->State==ENinjaMatchState::Ready)
	{
		HoldEnemies(); Match->Confirm(); Next(TEXT("Second retry prepares another fresh round"));
	}
	else if (Phase==9 && Match->CanPlay() && Player->GetTime().GetTotalSeconds()>.3)
	{
		for (int32 I=0;I<Match->Enemies.Num()-1;++I) Match->Enemies[I]->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja);
		Player->OnEndReached.AddDynamic(this,&ANinjaMusicCheck::DefeatAtSongEnd);
		if (!Player->Seek(Player->GetDuration()-FTimespan::FromMilliseconds(600))) { Finish(TEXT("Deadline tie seek rejected")); return; }
		Next(TEXT("Arrange the final enemy defeat on the actual media-end event"));
	}
	else if (Phase==10 && Match->State==ENinjaMatchState::Won && Wall-PhaseWall>.4)
	{
		if (Player || !bDeadlineTieDefeated) { Finish(TEXT("Song-end victory priority or music stop failed")); return; }
		Cases.Add(TEXT("The last defeat wins a tied music deadline, stops playback and stays stable")); Finish(FString());
	}
}
void ANinjaMusicCheck::Finish(const FString& Error)
{
	UGameplayStatics::SetGamePaused(this,false);
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetBoolField(TEXT("full_track"),bFullTrack); Report->SetNumberField(TEXT("phase"),Phase); Report->SetNumberField(TEXT("peak_decoded_envelope"),PeakEnvelope);
	if (Match) { Report->SetNumberField(TEXT("duration_seconds"),Match->RoundDurationSeconds); Report->SetNumberField(TEXT("enemies"),Match->Enemies.Num()); }
	Report->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-Started);
	Report->SetNumberField(TEXT("first_track_wall_seconds_including_pause"),CompletedTrackWallSeconds);
	Report->SetStringField(TEXT("scope"),TEXT("Real WMF M4A playback and audio mixer, AI held stationary to isolate music lifecycle; no fixed simulation clock"));
	TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Case:Cases) Rows.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Rows);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("MusicReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("MUSIC_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
