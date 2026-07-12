# Tong-its

Tong-its is currently a **T-Display-only first pass**. It uses shared game logic so it can be reused later, but the C3 screen is too small for a useful version at the moment.

This implementation is a simplified three-player rummy game:

- You play against Bot A and Bot B.
- Each player starts with 12 cards.
- There is a stock pile and a face-up discard pile.
- Your turn is always: draw one card, optionally expose a meld, then discard one card.
- A meld is either a set of three or more cards of the same rank, or a run of three or more cards in the same suit.
- You can lay off, or `sapaw`, by adding a card from your hand to an exposed meld already on the table.
- The game ends if a player runs out of cards, or if the stock pile runs out.
- If the stock runs out, the lowest deadwood score wins.
- Deadwood is the point value of the cards still in your hand. Aces count 1, number cards count face value, and 10/J/Q/K count 10.

## Controls

- `B1 tap`: move to the next selectable thing on the current screen.
- `B1 hold`: perform the current action.
- `B2 tap`: switch screens.
- `B2 hold`: exit back to the MicriOS menu.

## Screens

Tong-its has four screens. The label near the top shows the current screen and turn phase, such as `Hand / Draw` or `Piles / Act`.

### Hand

Shows your cards.

During `Draw` phase:

- You cannot discard yet.
- Use `B2 tap` to switch to `Piles`.
- Holding `B1` from here draws from the stock pile as a shortcut.

During `Act` phase:

- `B1 tap` moves the highlighted card.
- `B1 hold` discards the highlighted card.
- After you discard, both bots take their turns and play returns to your draw phase.

### Piles

Shows the stock pile and the discard pile.

During `Draw` phase:

- `B1 tap` toggles between stock and discard.
- `B1 hold` draws from the selected pile.
- The discard pile can only be drawn if that discard card can immediately make and expose a meld.

During `Act` phase:

- This screen is informational only. Switch back to `Hand` to discard, or `Melds` to expose a meld.

### Melds

Shows melds the game thinks you can expose.

- `B1 tap` cycles through available meld suggestions.
- `B1 hold` exposes the selected meld.
- If it says `No meld ready`, you do not currently have a set or run the simplified helper can expose.

After exposing a meld, you still need to discard from your hand unless the meld emptied your hand.

### Table / Sapaw

Shows exposed melds on the table.

- First choose the card you want to lay off from the `Hand` screen.
- `B1 tap` moves forward through exposed melds.
- `B2 tap` moves backward through exposed melds.
- `B1 hold` tries to place your selected hand card onto the selected meld.
- `B1 hold` on `Exit meld view` returns to your hand.
- `B2 hold` also returns to your hand.

The bottom-right status says `fits` when the selected hand card can be added to the highlighted meld. It says `no fit` when that card cannot be placed there.

## How To Play A Turn

1. Start on `Piles / Draw`, or press `B2` until you see `Piles`.
2. Tap `B1` to choose stock or discard.
3. Hold `B1` to draw.
4. If you have a meld, press `B2` until `Melds`, then hold `B1` to expose it.
5. If you want to lay off a card, go to `Hand`, highlight the card, then go to `Table` and hold `B1` on a meld where the card fits.
6. Press `B2` or use the table exit row to return to `Hand`.
7. Tap `B1` until the card you want to discard is highlighted.
8. Hold `B1` to discard.
9. The bots play automatically, then it becomes your draw phase again.

## Current Limitations

This is not a complete tournament Tong-its implementation yet.

- No challenge/fight/draw-pot side rules.
- Sapaw is limited to adding one selected hand card to the end of a valid set/run. It does not yet support more advanced table browsing or multi-card layoff in one action.
- Discard-pile pickup must now immediately expose a meld containing the discard, but the game still omits some table-money and challenge-related discard consequences.
- Bots use simple heuristics.
- There is no scoring or saved high score yet.
