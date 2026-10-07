import path from 'node:path';

// The compiler emits package-local imports as quoted sibling includes. That spelling is correct only
// while the generated tree is the sole include tree: C++ searches the including file's directory before
// every -I directory for a quoted include, so a generated sibling silently bypasses a hand-written
// override with the same installed path.
//
// Resolve only includes whose target is another emitted header, and spell that already-decided target by
// its canonical installed path. This changes no dependency and introduces no declaration or behavior; it
// makes nested includes obey the same include-path precedence as a consumer's top-level include. A quote
// that does not resolve inside the emitted inventory is left byte-for-byte unchanged.
export function canonicalizeGeneratedIncludes(files) {
  const emitted = new Set(files.map((file) => portable(file.path)));
  const changedFiles = [];
  let includes = 0;

  for (const file of files) {
    const source = typeof file.contents === 'string' ? file.contents : String(file.contents);
    const directory = path.posix.dirname(portable(file.path));
    let changed = 0;
    const contents = source.replace(/^#include "([^"\r\n]+)"$/gmu, (line, relative) => {
      const resolved = path.posix.normalize(path.posix.join(directory, relative));
      if (!emitted.has(resolved)) return line;
      changed += 1;
      return `#include <${resolved}>`;
    });
    if (changed === 0) continue;
    file.contents = contents;
    changedFiles.push(portable(file.path));
    includes += changed;
  }

  return { files: changedFiles.sort(), includes };
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}
