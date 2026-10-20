#pragma once
#include <Arduino.h>
void beginMotor();
bool startFeed(uint32_t duration);
void stopFeed();
void stopFeedIfSequence(uint32_t sequence);
void tickMotor();
bool motorIsFeeding();
uint32_t motorFeedSequence();
