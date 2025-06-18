<!-- omit in toc -->
# Open-source software (OSS) template

This repository template provides a good starting point for contributions that aim to be published as open source. For instructions on creating a repository from a template, please see the [GitHub documentation](https://docs.github.com/repositories/creating-and-managing-repositories/creating-a-repository-from-a-template).

<!-- omit in toc -->
## Table of contents

- [Style](#style)
    - [Visual Studio Code integration](#visual-studio-code-integration)
        - [General extensions](#general-extensions)
        - [Markdown extensions](#markdown-extensions)
        - [YAML extensions](#yaml-extensions)
- [Assets](#assets)
- [Open source checklist](#open-source-checklist)

## Style

The following tools have been set up to enforce a common code style:

- [EditorConfig](https://editorconfig.org/) - Helps maintain consistent coding styles for multiple developers working on the same project across various editors and IDEs.
- [super-linter](https://github.com/github/super-linter) - Combination of multiple linters, running as a GitHub Action.

### Visual Studio Code integration

The repository has been set up to integrate with [Visual Studio Code](https://code.visualstudio.com/). The following extensions are recommended when opening the repository.

#### General extensions

- [EditorConfig for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=editorconfig.editorconfig) - Extension automatically aligns the content of a file with the style defined in `.editorconfig` when file is saved.
- [Code Spell Checker](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker) - Extension visually spell checks your code or documentation as you type. The language `en-US` is used by default.

#### Markdown extensions

- [markdownlint](https://marketplace.visualstudio.com/items?itemName=DavidAnson.vscode-markdownlint) - Configuration in `.vscode/settings.json` instructs the extension to automatically align the content of a file with the style defined in `.markdownlint.yml` when file is saved.
- [Markdown All in One](https://marketplace.visualstudio.com/items?itemName=yzhang.markdown-all-in-one) - Configuration in `.vscode/settings.json` instructs the extension to automate some of the tedious tasks in Markdown, like updating the table of contents (ToC) to match the chapters in a document.

#### YAML extensions

- [YAML](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml) - Configuration in `.vscode/settings.json` instructs the extension to visually validate all GitHub Action workflows. This aligns with the *shift left* strategy where the environment is set up to find issues as soon as possible.

## Assets

The `assets` directory can be used e.g. for storing images that are referenced from Markdown documentation. The file `assets/.gitkeep` can be removed once other files are stored in the directory.

## Open source checklist

Please complete the following work items before starting the open source process:

- [ ] `CODEOWNERS` - Create the [code owners file](https://docs.github.com/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners) to define the individuals or teams that are responsible for the code in the repository.
- [ ] `CONTRIBUTING.md` - The contributing file is included in the [community profiles for public repositories](https://docs.github.com/communities/setting-up-your-project-for-healthy-contributions/about-community-profiles-for-public-repositories). The file in this repository is a template, where you will need to replace all occurrences of `REPO_NAME` with the name of your repository in question.
- [ ] `LICENSE` - Create the [license file](https://docs.github.com/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) to tells others what they can and can't do with your source code. Generally we use [Apache 2.0](https://choosealicense.com/licenses/apache-2.0/) or [MIT](https://choosealicense.com/licenses/mit/) for our public contributions.
- [ ] Align with the [open source contribution guidelines](https://github.com/AxisCommunications/github-getting-started/tree/main/open-source-contributions).
