(() => {
  "use strict";

  const post = (message) => {
    try { window.chrome.webview.postMessage(message); } catch (_) { /* host unavailable */ }
  };

  const content = document.getElementById("reader-content");
  const progress = document.getElementById("reading-progress");
  const toc = document.getElementById("reader-toc");
  let scrollQueued = false;

  const scrollRatio = () => {
    const max = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    return max === 0 ? 0 : Math.min(1, Math.max(0, window.scrollY / max));
  };

  const reportScroll = () => {
    scrollQueued = false;
    const ratio = scrollRatio();
    progress.style.width = `${(ratio * 100).toFixed(2)}%`;
    post(`scroll:${ratio.toFixed(6)}`);
  };

  window.addEventListener("scroll", () => {
    if (!scrollQueued) {
      scrollQueued = true;
      window.requestAnimationFrame(reportScroll);
    }
  }, { passive: true });

  const slugCounts = new Map();
  const slugFor = (text) => {
    const base = text.trim().toLowerCase()
      .normalize("NFKD")
      .replace(/[\u0300-\u036f]/g, "")
      .replace(/[^\p{Letter}\p{Number}\s-]/gu, "")
      .replace(/\s+/g, "-")
      .replace(/-+/g, "-")
      .replace(/^-|-$/g, "") || "section";
    const count = slugCounts.get(base) || 0;
    slugCounts.set(base, count + 1);
    return count === 0 ? base : `${base}-${count + 1}`;
  };

  const headings = [...content.querySelectorAll("h1, h2, h3, h4")];
  for (const heading of headings) {
    if (!heading.id) heading.id = slugFor(heading.textContent || "section");
    const anchor = document.createElement("a");
    anchor.className = "heading-anchor";
    anchor.href = `#${encodeURIComponent(heading.id)}`;
    anchor.setAttribute("aria-label", "Link to this section");
    anchor.textContent = "#";
    heading.prepend(anchor);
  }

  const tocHeadings = headings.filter((heading) => heading.tagName === "H2" || heading.tagName === "H3");
  if (tocHeadings.length >= 3) {
    const title = document.createElement("div");
    title.className = "toc-title";
    title.textContent = "On this page";
    toc.append(title);
    for (const heading of tocHeadings) {
      const link = document.createElement("a");
      link.href = `#${encodeURIComponent(heading.id)}`;
      link.dataset.level = heading.tagName.substring(1);
      link.textContent = heading.textContent.replace(/^#/, "").trim();
      toc.append(link);
    }
    toc.dataset.visible = "true";

    const links = [...toc.querySelectorAll("a")];
    const observer = new IntersectionObserver((entries) => {
      const visible = entries.filter((entry) => entry.isIntersecting)
        .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top)[0];
      if (!visible) return;
      for (const link of links) link.classList.toggle("active", link.hash === `#${visible.target.id}`);
    }, { rootMargin: "-15% 0px -72% 0px", threshold: 0 });
    tocHeadings.forEach((heading) => observer.observe(heading));
  }

  for (const table of content.querySelectorAll("table")) {
    if (table.parentElement?.classList.contains("table-wrap")) continue;
    const wrap = document.createElement("div");
    wrap.className = "table-wrap";
    table.replaceWith(wrap);
    wrap.append(table);
  }

  const copyText = async (text) => {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(text);
      return;
    }
    const area = document.createElement("textarea");
    area.value = text;
    area.setAttribute("readonly", "");
    area.style.position = "fixed";
    area.style.opacity = "0";
    document.body.append(area);
    area.select();
    const copied = document.execCommand("copy");
    area.remove();
    if (!copied) throw new Error("Copy command was rejected");
  };

  for (const pre of content.querySelectorAll("pre")) {
    const code = pre.querySelector("code");
    if (!code) continue;
    const languageClass = [...code.classList].find((name) => name.startsWith("language-"));
    if (languageClass) {
      const label = document.createElement("span");
      label.className = "code-language";
      label.textContent = languageClass.substring("language-".length);
      pre.append(label);
    }
    const button = document.createElement("button");
    button.type = "button";
    button.className = "copy-code";
    button.textContent = "Copy";
    button.addEventListener("click", async () => {
      try {
        await copyText(code.innerText);
        button.textContent = "Copied";
        window.setTimeout(() => { button.textContent = "Copy"; }, 1200);
      } catch (_) {
        button.textContent = "Copy failed";
        window.setTimeout(() => { button.textContent = "Copy"; }, 1600);
      }
    });
    pre.append(button);
  }

  for (const image of content.querySelectorAll("img")) {
    image.addEventListener("error", () => {
      const replacement = document.createElement("div");
      replacement.className = "image-error";
      replacement.textContent = image.alt ? `Image unavailable: ${image.alt}` : "Image unavailable";
      image.replaceWith(replacement);
    }, { once: true });
  }

  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" || (event.ctrlKey && event.altKey && event.key.toLowerCase() === "m")) {
      event.preventDefault();
      post("exit");
      return;
    }
    if (event.ctrlKey && !event.altKey && !event.metaKey && event.key.toLowerCase() === "s") {
      event.preventDefault();
      post(event.shiftKey ? "save-as" : "save");
    }
  });

  const initial = Number.parseFloat(document.body.dataset.scroll || "0");
  const restoreScroll = () => {
    const max = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    window.scrollTo({ top: Math.max(0, Math.min(1, initial)) * max, behavior: "auto" });
    reportScroll();
  };
  window.requestAnimationFrame(restoreScroll);
  window.addEventListener("load", () => window.setTimeout(restoreScroll, 0), { once: true });
})();
