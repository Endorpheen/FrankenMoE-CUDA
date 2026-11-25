"""Emit a deterministic English corpus for the Bonsai parity benchmark."""
questions = (
    "How does a clock keep time when temperature changes?",
    "Why does a long bridge need joints that can expand?",
    "How can a town conserve water through a dry summer?",
    "What makes batteries lose capacity in cold weather?",
    "Why do some birds navigate by following the stars?",
    "How do engineers compare two measurements with different uncertainty?",
    "Why is a shaded pond cooler than a paved courtyard?",
    "Which tests distinguish a software error from a faulty sensor?",
)
for index in range(1800):
    question = questions[index % len(questions)]
    print(f"Field note {index + 1:04d}: {question}")
    print("Start with a measurable quantity. Describe what you would hold constant, "
          "the conditions to vary, and one observation that would challenge your explanation.")
