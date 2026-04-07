#include "bessel.hpp"

Bessel::Bessel()
{
	update();
}

float Bessel::calculateBessel(int order, float x) 
{
	const float EPS = 1e-8;
	const int MAX_TERMS = 20;

	// J0
	if (order == 0)
	{
		float sum = 0.0;
		float term = 1.0;
		int k = 0;

		do
		{
			term = std::pow(-1.0, k) *
				std::pow(x / 2.0, 2 * k) /
				(factorial(k) * factorial(k));

			sum += term;
			k++;
		} while (std::fabs(term) > EPS && k < MAX_TERMS);

		return sum;
	}

	// J1
	if (order == 1)
	{
		float sum = 0.0;
		float term = 1.0;
		int k = 0;

		do {
			term = std::pow(-1.0, k) *
				std::pow(x / 2.0, 2 * k + 1) /
				(factorial(k) * factorial(k + 1));

			sum += term;
			k++;

		} while (std::fabs(term) > EPS && k < MAX_TERMS);

		return sum;
	}

	// higher orders (recurrence)
	if (x == 0.0)
		return 0.0;

	return (2.0 * (order - 1) / x) * calculateBessel(order - 1, x)
		- calculateBessel(order - 2, x);
}

void Bessel::calculateWeights()
{
	for (int i = 0; i < MAX_MODES; i++)
	{
		const Root& r = roots[i];
		float weight = calculateBessel(r.order, r.value * position);
		weights[i] = scale(weight, 0.f, 1.f, overtones, 1, damping);
	}
}

void Bessel::calculateFreqs()
{
	for (int i = 0; i < MAX_MODES; i++)
	{
		const Root& r = roots[i];
		freqs[i] = std::max(20.f, std::min((r.value * fundamentalPitch) / size, samplerate * 0.25f));
	}

	/*
	const float root0 = roots[0].value;
	for (int i = 0; i < MAX_MODES; i++)
	{
		freqs[i] = fundamentalPitch * (roots[i].value / root0);
		freqs[i] = std::max(20.f, std::min(freqs[i], samplerate * 0.25f));
	}
	*/
}

void Bessel::update()
{
	calculateWeights();
	calculateFreqs();
}


void Bessel::setSamplerate(int newSamplerate)
{
	samplerate = newSamplerate;
}

void Bessel::setPitch(float newFreq)
{
	fundamentalPitch = std::max(20.f, std::min(newFreq, 10000.f));
}

void Bessel::setPosition(float newPosition)
{
	position = std::max(0.f, std::min(newPosition, 1.f));
}

void Bessel::setSize(float newSize)
{
	size = std::max(0.0001f, std::min(newSize, 5.f));
}

void Bessel::setDamping(float newDamping)
{
	damping = std::max(0.f, std::min(newDamping, 1.f));
}

void Bessel::setOvertones(float newOvertones)
{
	overtones = std::max(0.f, std::min(newOvertones, 1.f));
}


float Bessel::getWeight(int index)
{
	int i = std::max(0, std::min(index, MAX_MODES - 1));
	return weights[i];
}

float Bessel::getFreq(int index)
{
	int i = std::max(0, std::min(index, MAX_MODES - 1));
	return freqs[i];
}