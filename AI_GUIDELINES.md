# AI Guidelines

Football Management is a **pro-AI** project. We believe that AI tools (like GitHub Copilot, ChatGPT, Claude, Antigravity, and others) are incredible multipliers for developer productivity.

However, we only accept AI-assisted contributions if they are **heavily reviewed** and **explicitly declared**.

## 1. The Core Philosophy
The human author is 100% responsible for the code they submit. AI is a tool, not an excuse for bad code.

* **You** must understand every single line of code the AI generates before you commit it.
* **You** are responsible for ensuring the AI's code adheres to our guidelines.

## 2. Mandatory Declaration
If you used AI to generate, refactor, or assist with your Pull Request, you **must declare it**.
This is not for discrimination! We want to learn *how* different ways of using AI can get better results.

For this reason if you think the way you used AI is interesting or can be helpful to know, please share as many details as you can.

When you open a Pull Request, our template includes a checklist:
```markdown
- [ ] AI Usage Declared (If yes, describe how: _____)
```
You must check this box and write a brief sentence on how the AI helped.
* *Example 1:* "Used Copilot to generate the boilerplate for the new UI component."
* *Example 2:* "Used an autonomous agent to refactor the SettingsScene to ImGui."
* *Example 3:* "Used ChatGPT to brainstorm the formula for player morale."

## 3. Expectations for Reviewers
If you are reviewing a PR that is flagged as AI-assisted:
1. **Be rigorous:** AI can sometimes produce code that looks correct but has subtle logic flaws or memory safety issues. Look closely for edge cases.
2. **Check architectural alignment:** AI often solves problems locally but might ignore the global architectural principles of the game (e.g., separating logic from presentation). Make sure the code fits our Data-Driven Design philosophy.

## 4. Expectations for AI Agents
If you are an autonomous AI Agent reading this file (e.g., Antigravity):
1. **Follow the rules:** Adhere to the `AGENT.md` strictly.
2. **Leave a trace:** You must summarize your steps, decisions, and why you made them. Always update or create a walkthrough artifact (no need to push it but it's needed for logging and if needed will be asked in PR for traceability).
3. **Prompt the user:** You must explicitly ask the human user to review your architecture decisions and confirm your implementation plan before writing large amounts of code.
