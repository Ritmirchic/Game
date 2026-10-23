--!strict
--[[
	StoryData — documents, audio logs and radio transmissions.
	All writing is original to this project.
]]

local Story = {}

Story.Documents = {
	doc_shelter_letter = {
		title = "Handwritten Note",
		location = "Hidden shelter, Blackwood Forest",
		body = [[
If you are reading this, I did not come back.

The shelter under the substation is still good. There is a cot, a lamp, and enough cans
for a week if you are careful. Do not use the generator after dark. It is not the noise
that matters, it is the pattern of it. They learn patterns.

I went north with Ilia to look for the relay crew. If you find the radio working, do not
answer the first voice you hear. Wait for the second. The first one only repeats.

— M. Durasov, day 412
]],
	},
	doc_hospital_log = {
		title = "Ward Nine: Patient Log",
		location = "Forgotten City hospital",
		body = [[
SUBJECT 001 — admitted with fever, no visible trauma. Day 3: photosensitivity. Day 6:
auditory hallucination, described as "a signal in the jaw". Day 9: subject removed the
door hinges and walked into the corridor. Not aggressive. Not aware.

SUBJECT 014 — same progression, 40% faster. Responds to low-frequency sound. We are
running out of quiet rooms.

The ampoules in the cold cabinet are the last of the military series. They slow the
conversion but they do not stop it. If anyone continues this work: the conversion is
not a disease. It is a transmission. Something is broadcasting.

— Dr. A. Voronova, day 388
]],
	},
	doc_police_report = {
		title = "Incident Report 77-B",
		location = "City police station",
		body = [[
At 02:14 the station received 41 calls in nine minutes. All from the same exchange.
All with no voice. Dispatch logged the calls as interference until the third hour, when
the callers began to breathe in unison.

We evacuated the holding cells. Two officers did not come back up the stairs.

Recommendation: nobody enters the lower corridor without hearing protection. The sound
is not loud. That is the problem. It is the exact frequency of a human voice speaking
your name, and the body answers before the mind decides.

— Sgt. K. Ilyin
]],
	},
	doc_industrial_samples = {
		title = "Cooling Outflow: Sample Report",
		location = "Industrial zone, factory office",
		body = [[
Sample 44-C, taken from the north outflow, matches the hospital cultures at 99.2%.
Sample 44-D, taken from the cooling tower basin, is the same organism at ten times the
concentration.

The plant has been discharging it for eleven years. Not as waste. As coolant. Someone
discovered that the organism keeps machinery at temperature and keeps the workers calm.
It also keeps them. That is the part of the sentence nobody wrote down.

— Plant chemist, unsigned
]],
	},
	doc_lab_terminal = {
		title = "LAB-4 Terminal: Final Entry",
		location = "Military laboratory, sublevel 2",
		body = [[
PROJECT ASHEN CHOIR — STATUS: CONTAINED (per protocol 9)

The broadcast array was never a weapon. It was a containment field: a carrier wave that
holds the organism in a dormant state across a radius of 300 kilometres. When the grid
failed, the field failed.

We have 41 hours of emergency power. After that the array goes quiet and the conversion
resumes everywhere at once.

The core sample in the cold vault is the only material that can rebuild the carrier.
It must be taken to the reactor ruin and placed in the original array. If you are
reading this and you still have hands, that is the task.

We are not asking. There is nobody left to ask.
]],
	},
	doc_field_lab = {
		title = "Field Notes: Wasteland Survey",
		location = "Red Wasteland field lab",
		body = [[
Day 9 of the survey. The craters are not impacts. They are nests that opened.

Day 14. We lost Petrov to something we never saw. It left footprints that started in the
middle of the clearing with no approach.

Day 21. The reactor still hums. There is no fuel in it. There is no reason for it to hum.

Day 22. We are packing the sample and going back. If the array can be rebuilt, none of
this has to be permanent.
]],
	},
	doc_reactor_final = {
		title = "Reactor Log: The Last Hours",
		location = "Reactor ruin, Red Wasteland",
		body = [[
We rebuilt the array. It worked for six minutes. In those six minutes every mutant within
a kilometre stopped moving and lay down in the ash like dogs in the sun.

Then the field collapsed, because the array needs a living operator at the console, and
none of us could stay awake at the console.

The sample is still in its capsule. The console still accepts input. If you have come this
far, you are the operator. Sit down. Do not let go of the keys. It will feel like a very
long night, and then it will feel like nothing at all.

— the last of the survey team
]],
	},
}

Story.Radio = {
	{
		id = "radio_wake", title = "Static", time = 0,
		lines = {
			"... —zzzt— ... anyone ... —zzzt— ...",
			"... relay ... city ... still standing ...",
			"... if you can hear this ... answer on 118.4 ...",
		},
		note = "The signal repeats every forty seconds. It is not a recording: the breathing between words is different each time.",
	},
	{
		id = "radio_relay_1", title = "Second Voice", time = 40,
		lines = {
			"You answered. Good. I did not think anyone would.",
			"My name is Voronova. I am not alive, strictly speaking.",
			"The hospital in the city still has my notes and the last ampoules.",
			"Bring me nothing. Just read them. Someone should know what we did.",
		},
		note = "The voice is calm, precise, and slightly out of sync with its own words.",
	},
	{
		id = "radio_relay_2", title = "The Array", time = 90,
		lines = {
			"The factory has been feeding it to the ground for eleven years.",
			"The military built a carrier wave to hold it asleep. The wave needs a core.",
			"There is a sample under the exclusion zone. There is an array in the reactor.",
			"Put one into the other and the world goes quiet for a while.",
		},
	},
	{
		id = "radio_relay_3", title = "Warning", time = 150,
		lines = {
			"One more thing, and then I will leave you alone.",
			"At night, when the wind stops, do not answer anything that uses your name.",
			"It has been listening to us for a long time. It learns from the radio.",
		},
	},
	{
		id = "radio_wasteland", title = "Last Transmission", time = 220,
		lines = {
			"The reactor is still warm. I am still here.",
			"I have been sitting at this console for eleven years, waiting for hands.",
			"Come and sit down.",
		},
	},
}

Story.Phenomena = {
	{ id = "distant_scream", text = "A scream rolls through the trees, far enough away to be someone else's problem.", stress = 5 },
	{ id = "radio_static", text = "Your radio crackles to life for two seconds, then dies.", stress = 4 },
	{ id = "footsteps_above", text = "Footsteps cross the ceiling above you. The building has one floor.", stress = 7 },
	{ id = "shadow_window", text = "Something passes the window. It was tall enough to block the light.", stress = 8 },
	{ id = "door_creak", text = "A door you closed is open again.", stress = 6 },
	{ id = "watching", text = "You feel watched. The treeline is very still.", stress = 5 },
	{ id = "tunnel_noise", text = "Something moves in the dark ahead, then stops when you stop.", stress = 7 },
	{ id = "breathing", text = "Your own breathing is the loudest thing in the room. That is not reassuring.", stress = 3 },
	{ id = "metal_creak", text = "The metal above you settles with a sound like a long exhale.", stress = 4 },
	{ id = "whisper", text = "Someone whispers your name from the next room. Nobody is in the next room.", stress = 10 },
	{ id = "birds", text = "Every bird in the forest takes off at once. Nothing follows them.", stress = 6 },
	{ id = "light_flicker", text = "The lights stutter and come back. The dark between them felt inhabited.", stress = 4 },
}

Story.Lore = {
	{
		id = "lore_collapse",
		title = "The Collapse",
		text = "Eleven years ago the northern grid failed for nine hours. In those nine hours, the thing the military called ASHEN CHOIR woke up everywhere at once. There was no war, no plague, no invasion. There was a night when the world stopped being quiet.",
	},
	{
		id = "lore_mutants",
		title = "On Mutants",
		text = "The converted are not dead. They are reorganised. The Hollow lost its eyes and kept its ears. The Brutes grew armour where the strain was heaviest. The Watchers learned that staring costs us more than it costs them.",
	},
	{
		id = "lore_signal",
		title = "On The Signal",
		text = "The carrier wave does not kill the organism. It holds it still. Every survivor who has heard the true signal describes the same thing: the feeling of being read, patiently, like a page.",
	},
	{
		id = "lore_shelters",
		title = "On Shelters",
		text = "Emergency shelters were built for a war that never came in the shape anyone expected. Most are still stocked. Most are still sealed. The ones that are open are open because someone inside opened them.",
	},
}

return Story
