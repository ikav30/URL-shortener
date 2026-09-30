# Project Brief — Smart URL Shortener with Built-in Rate Limiting

## What are we making?
A **URL shortening service** — a web service that takes a long, messy link and
turns it into a short, clean one.

**Example:**
- Input:  `https://www.zomato.com/bangalore/restaurants/north-indian?filters=rating&page=3&ref=homepage`
- Output: `zmt.co/aX9k2`
- Anyone who visits `zmt.co/aX9k2` gets instantly redirected to the original long link.

On top of that core, we're adding two things most student versions skip:
1. **Analytics** — track how many people clicked, when, from where, and on what device.
2. **A rate limiter** — a guard that stops any single user from abusing the service
   by creating thousands of links per second.

## What is it used for? (Real-world uses)
- **Social media & marketing** — keep posts clean, save characters.
- **Analytics & tracking** — measure how many people clicked a campaign/email/ad link.
- **QR codes** — short URLs fit into small, reliable, scannable QR codes.
- **SMS & notifications** — order-tracking links (Zomato/Swiggy/Amazon) stay short and trustworthy.
- **Hiding ugly/complex links** — clean links are more clickable and shareable.

## Why are we making it?
1. It solves a genuine, well-understood problem (not a throwaway toy).
2. It's a proven system-design interview question ("Design TinyURL"), asked at
   Amazon, Google, and many others. Building it doubles as interview prep.

## What problem does it solve?
| Problem | How our service solves it |
|---------|--------------------------|
| Long URLs are ugly & unshareable | Converts them to short, clean, memorable links |
| You can't measure link engagement | Built-in analytics track every click (count, time, location, device) |
| Long URLs break QR codes & SMS | Short links fit cleanly into both |
| Services get abused by bots/spammers | The rate limiter blocks anyone hammering the create endpoint |
| Redirects must be near-instant at scale | A Redis cache serves popular links in milliseconds |

## How will it help? (The value it delivers)
- **End user:** clean shareable links + a dashboard showing how their links perform.
- **System:** stays fast (caching), fair (rate limiting), and stable under heavy traffic.
- **You (the real goal):** one deployed, demo-able project that lets you speak fluently about
  caching, database indexing, unique-ID generation, concurrency, and abuse prevention.

## The engineering challenge (why it's not trivial)
- **Read-heavy:** a link is created once but clicked thousands of times → reads must be
  blazing fast → **caching**.
- **Uniqueness:** every short code must be unique and never collide, even across
  multiple servers → **smart ID generation (Base62)**.
- **Abuse resistance:** the service must survive abuse without falling over → **rate limiting**.
- **Scale:** it must stay fast when traffic spikes → the whole architecture is designed around this.

The gap — "simple idea, hard to do well at scale" — is exactly what makes it resume-worthy.
